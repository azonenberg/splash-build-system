/***********************************************************************************************************************
*                                                                                                                      *
* SPLASH build system v0.2                                                                                             *
*                                                                                                                      *
* Copyright (c) 2016 Andrew D. Zonenberg                                                                               *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

#include "splashbuild.h"

using namespace std;

void ShowUsage();
void ShowVersion();

//map of toolchain hashes to toolchain objects
map<string, Toolchain*> g_toolchains;

void CleanBuildDir();
void ProcessDependencyScan(Socket& sock, DependencyScan rxm);
void ProcessBuildRequest(Socket& sock, const NodeBuildRequest& rxm);
Toolchain* PrepBuild(string toolhash);
bool RefreshCachedFile(Socket& sock, string hash, string fname);
bool GrabSourceFile(Socket& sock, string fname, string hash);

bool DoScanDependencies(
	Socket& sock,
	Toolchain* chain,
	const set<BuildFlag>& flags,
	string arch,
	string aname,
	string& output,
	set<BuildFlag>& libFlags,
	DependencyResults* replym);

bool GrabMissingDependencies(
	Socket& sock,
	set<string> missingFiles,
	string& errors);

//Temporary directory we work in
string g_tmpdir;

//Temporary directory we do builds in
string g_builddir;

/**
	@brief Program entry point
 */
int main(int argc, char* argv[])
{
	Severity console_verbosity = Severity::NOTICE;

	string ctl_server;
	int port = 49000;
	int nodenum = 0;
	string uuid;

	//Parse command-line arguments
	for(int i=1; i<argc; i++)
	{
		string s(argv[i]);

		//Let the logger eat its args first
		if(ParseLoggerArguments(i, argc, argv, console_verbosity))
			continue;

		else if(s == "--help")
		{
			ShowUsage();
			return 0;
		}

		else if(s == "--version")
		{
			ShowVersion();
			return 0;
		}

		else if( (s == "--nodenum") && (i+1 < argc) )
			nodenum = atoi(argv[++i]);

		else if( (s == "--uuid") && (i+1 < argc) )
			uuid = argv[++i];

		//Last arg without switch is control server.
		//TODO: mandatory arguments to introduce these?
		else
			ctl_server = argv[i];

	}

	char sworker[64];
	snprintf(sworker, sizeof(sworker), "sb%d", nodenum);

	//Set up temporary directory
	g_tmpdir = string("/tmp/") + sworker;
	MakeDirectoryRecursive(g_tmpdir, 0700);
	g_builddir = g_tmpdir + "/workdir";
	MakeDirectoryRecursive(g_builddir, 0700);

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	//Print header
	if(console_verbosity >= Severity::NOTICE)
	{
		ShowVersion();
		printf("\n");
	}

	//Initialize the cache
	//Use separate caches for each instance if we multithread for now.
	//TODO: figure out how to share?
	g_cache = new Cache(sworker);

	//Set up the config object from our arguments
	g_clientSettings = new ClientSettings(ctl_server, port, uuid);

	//Connect to the server
	Socket sock(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if(!ConnectToServer(sock, ClientHello::CLIENT_BUILD, string("-") + sworker))
		return 1;

	//Look for compilers
	LogVerbose("Enumerating compilers...\n");
	{
		LogIndenter li;
		FindLinkers();
		FindCPPCompilers();
		FindFPGACompilers();
	}

	//Get some basic metadata about our hardware and tell the server
	SplashMsg binfo;
	auto binfom = binfo.mutable_buildinfo();
	binfom->set_cpucount(atoi(ShellCommand("cat /proc/cpuinfo  | grep processor | wc -l").c_str()));
	binfom->set_cpuspeed(atoi(ShellCommand("cat /proc/cpuinfo | grep bogo | head -n 1 | cut -d : -f 2").c_str()));
	binfom->set_ramsize(atol(ShellCommand("cat /proc/meminfo  | grep MemTotal  | cut -d : -f 2").c_str()) / 1024);
	binfom->set_numchains(g_toolchains.size());
	if(!SendMessage(sock, binfo, ctl_server))
		return 1;

	//Report the toolchains we found to the server
	for(auto it : g_toolchains)
	{
		auto t = it.second;

		//DEBUG: Print it out
		//t->DebugPrint();

		//Send the toolchain info to the server
		SplashMsg tadd;
		auto madd = tadd.mutable_addcompiler();
		madd->set_compilertype(t->GetType());
		madd->set_versionnum((t->GetMajorVersion() << 16) |
							(t->GetMinorVersion() << 8) |
							(t->GetPatchVersion() << 0));
		madd->set_hash(t->GetHash());
		madd->set_versionstr(t->GetVersionString());

		auto& fix = t->GetFixes();
		for(auto jt : fix)
		{
			auto type = madd->add_types();
			type->set_name(jt.first);
			type->set_prefix(jt.second.first);
			type->set_suffix(jt.second.second);
		}
		auto dlangs = t->GetSupportedLanguages();
		for(auto x : dlangs)
			madd->add_lang(x);
		auto triplets = t->GetTargetTriplets();
		for(auto x : triplets)
			*madd->add_triplet() = x;

		if(!SendMessage(sock, tadd, ctl_server))
			return 1;
	}

	//Sit around and wait for stuff to come in
	LogVerbose("\nReady\n\n");
	while(true)
	{
		SplashMsg rxm;
		if(!RecvMessage(sock, rxm))
			return 1;

		auto type = rxm.Payload_case();

		switch(type)
		{
			//Requesting a dependency scan
			case SplashMsg::kDependencyScan:
				ProcessDependencyScan(sock, rxm.dependencyscan());
				break;

			//Requesting a compile operation
			case SplashMsg::kNodeBuildRequest:
				ProcessBuildRequest(sock, rxm.nodebuildrequest());
				break;

			//Asking for more data
			case SplashMsg::kContentRequestByHash:
				if(!ProcessContentRequest(sock, g_clientSettings->GetServerHostname(), rxm))
					return false;
				break;

			default:
				LogDebug("Got an unknown message, ignoring it\n");
				break;
		}
	}

	//clean up
	delete g_cache;
	for(auto x : g_toolchains)
		delete x.second;
	g_toolchains.clear();
	delete g_clientSettings;

	return 0;
}

/**
	@brief Delete all files in the build directory
 */
void CleanBuildDir()
{
	//Verify there are no spaces (TODO: escape them?)
	if(g_builddir.find(" ") != string::npos)
		LogFatal("Handling of spaces in g_builddir not implemented\n");

	string cmd = "rm -rf " + g_builddir + "/*";
	ShellCommand(cmd.c_str());
}

/**
	@brief Prepare for a build
 */
Toolchain* PrepBuild(string toolhash)
{
	//Look up the toolchain we need
	if(g_toolchains.find(toolhash) == g_toolchains.end())
	{
		LogError(
			"Server requested a toolchain that we do not have installed (hash %s)\n"
			"This should never happen.\n",
			toolhash.c_str());
		return NULL;
	}

	Toolchain* chain = g_toolchains[toolhash];
	//LogDebug("Toolchain: %s\n", chain->GetVersionString().c_str());

	//Make sure we have a clean slate to build in
	//LogDebug("Cleaning temp directory\n");
	CleanBuildDir();

	return chain;
}

/**
	@brief Make sure a particular file is in our cache
 */
bool RefreshCachedFile(Socket& sock, string hash, string fname)
{
	if(!ValidatePath(fname))
	{
		LogWarning("path %s failed to validate\n", fname.c_str());
		return false;
	}

	if(!g_cache->IsCached(hash))
	{
		//Ask for the file
		//LogDebug("Source file %s (%s) is not in cache yet\n", fname.c_str(), hash.c_str());
		string edat;
		if(!GetRemoteFileByHash(sock, g_clientSettings->GetServerHostname(), hash, edat))
			return false;
		g_cache->AddFile(fname, hash, sha256(edat), edat, "");
	}

	return true;
}

/**
	@brief Process a "dependency scan" message from a client
 */
void ProcessDependencyScan(Socket& sock, DependencyScan rxm)
{
	//LogDebug("Got a dependency scan request\n");
	//LogIndenter li;

	//Do setup stuff
	Toolchain* chain = PrepBuild(rxm.toolchain());
	if(!chain)
		return;

	//Get the relative path of the source file
	string fname = rxm.fname();
	string basename = GetBasenameOfFile(fname);
	string dir = GetDirOfFile(fname);
	string adir = g_builddir + "/" + dir;
	string aname = g_builddir + "/" + fname;

	if(!ValidatePath(fname))
	{
		LogWarning("path %s failed to validate\n", fname.c_str());
		return;
	}

	//Create the relative path as needed
	//LogDebug("    Making build directory %s for source file %s\n", adir.c_str(), basename.c_str());
	MakeDirectoryRecursive(adir, 0700);

	//See if we have the file in our local cache
	string hash = rxm.hash();
	if(!RefreshCachedFile(sock, hash, fname))
		return;

	//It's in cache now, read it
	string data = g_cache->ReadCachedFile(hash);

	//Write the source file
	//LogDebug("Writing source file %s\n", aname.c_str());
	if(!PutFileContents(aname, data))
		return;

	//Look up the flags
	set<BuildFlag> flags;
	for(int i=0; i<rxm.flags_size(); i++)
		flags.emplace(BuildFlag(rxm.flags(i)));

	//Format the return message
	SplashMsg reply;
	auto replym = reply.mutable_dependencyresults();

	//Do the actual scan (recursively scanning headers etc)
	string output;
	set<BuildFlag> libFlags;
	chdir(g_builddir.c_str());
	bool ok = DoScanDependencies(sock, chain, flags, rxm.arch(), aname, output, libFlags, replym);

	//Process the results
	for(auto lib : libFlags)
		replym->add_libflags(lib);
	replym->set_result(ok);
	replym->set_stdout(output);

	//Done, one way or another. Send it.
	SendMessage(sock, reply);
}

/**
	@brief Run a dependency scan on a file (after parsing the message etc)
 */
bool DoScanDependencies(
	Socket& sock,
	Toolchain* chain,
	const set<BuildFlag>& flags,
	string arch,
	string aname,
	string& output,
	set<BuildFlag>& libFlags,
	DependencyResults* replym)
{
	//LogDebug("DoScanDependencies for %s\n", aname.c_str());
	//LogIndenter li;

	//Run the scanner proper
	while(true)
	{
		//LogDebug("Iteration start\n");
		//LogIndenter li;

		set<string> deps;
		map<string, string> hashes;
		set<string> missingFiles;
		if(!chain->ScanDependencies(
			arch,
			aname,
			g_builddir,
			flags,
			deps,
			hashes,
			output,
			missingFiles,
			libFlags))
		{
			//trim off trailing newlines
			while(isspace(output[output.length() - 1]))
				output.resize(output.length() - 1);
			return false;
		}

		//Make sure every dependency we asked for is actually on disk!
		//If the scan results were cached, some of them may not be.
		//Note that magic system paths don't exist clientside, so skip them as false positives
		set<string> missingDeps;
		for(auto d : deps)
		{
			string dc = d;
			if(!CanonicalizePathThatMightNotExist(dc))
			{
				output += string("Couldn't canonicalize path ") + d + "\n";
				return false;
			}
			if( !DoesFileExist(dc) && (dc.find("__sys") == string::npos) )
				missingDeps.emplace(dc);
		}
		if(!missingDeps.empty())
		{
			if(!GrabMissingDependencies(sock, missingDeps, output))
			{
				output += "ERROR: GrabMissingDependencies(1) failed\n";
				return false;
			}
		}

		//If the scan found files we're missing, ask for them!
		if(!missingFiles.empty())
		{
			//LogDebug("Finding missing files for %s\n", aname.c_str());

			//Get the files
			if(!GrabMissingDependencies(sock, missingFiles, output))
			{
				output += "ERROR: GrabMissingDependencies(2) failed\n";
				return false;
			}

			//Scan each of the files we downloaded to see if they pulled in more stuff
			//This is not an 100% accurate scan (since we aren't using any #define's provided by the parent file)
			//but lets us pre-fetch some stuff quickly.
			for(auto f : missingFiles)
			{
				//Run the scan
				DependencyResults ignored;
				set<BuildFlag> ignoredLibFlags;
				string recursiveOutput;
				if(!DoScanDependencies(
					sock,
					chain,
					flags,
					arch,
					g_builddir + "/" + f,
					recursiveOutput,
					ignoredLibFlags,
					&ignored))
				{
					output += string("Dependency scan of included file ") + f + " failed:\n";
					output += recursiveOutput;
					return false;
				}
			}

			//Go back and scan this file again
			//LogDebug("Re-running scan due to %d missing files\n", (int)missingFiles.size());
			continue;
		}

		//Successful completion of the scan, crunch the results
		//LogDebug("Scan of %s completed (%zu dependencies)\n", aname.c_str(), deps.size());
		for(auto d : deps)
		{
			//if( (d.find(".so") != string::npos) || (d.find(".a") != string::npos) )
			//	LogDebug("Dep %s\n", d.c_str());

			auto rd = replym->add_deps();
			rd->set_fname(d);
			rd->set_hash(hashes[d]);
		}

		//All good by this point
		return true;
	}
}

bool GrabMissingDependencies(
	Socket& sock,
	set<string> missingFiles,
	string& errors)
{
	//LogDebug("Grabbing %d missing dependencies...\n", (int)missingFiles.size());

	//Look up the hashes
	map<string, string> hashes;
	auto hostname = g_clientSettings->GetServerHostname();
	if(!GetRemoteHashesByPath(sock, hostname, missingFiles, hashes))
	{
		errors = "Failed to get remote hashes";
		return false;
	}

	//If any of the files weren't found on the far side, fail the scan right now
	for(auto m : missingFiles)
	{
		if(hashes.find(m) == hashes.end())
		{
			errors = string("ERROR: Failed to find include file ") + m + "\n";
			return false;
		}
	}

	//Pull the files into the cache
	if(!RefreshRemoteFilesByHash(sock, hostname, hashes))
		return false;

	//Write out the files
	for(auto it : hashes)
	{
		auto fname = it.first;
		string data = g_cache->ReadCachedFile(it.second);

		//LogDebug("Writing source file %s (object ID %s)\n", fname.c_str(), it.second.c_str());

		fname = g_builddir + "/" + fname;
		MakeDirectoryRecursive(GetDirOfFile(fname), 0700);
		if(!PutFileContents(fname, data))
			return false;
	}

	return true;
}

bool GrabSourceFile(Socket& sock, string fname, string hash)
{
	//See if we have the file in our local cache
	if(!RefreshCachedFile(sock, hash, fname))
		return false;

	//Write it
	string path = g_builddir + "/" + GetDirOfFile(fname);
	MakeDirectoryRecursive(path, 0700);
	string data = g_cache->ReadCachedFile(hash);
	string fpath = g_builddir + "/" + fname;
	//LogDebug("Writing input file %s\n", fpath.c_str());
	if(!PutFileContents(fpath, data))
		return false;

	return true;
}

/**
	@brief Process a "build request" message from a client
 */
void ProcessBuildRequest(Socket& sock, const NodeBuildRequest& rxm)
{
	LogDebug("Build request\n");
	LogIndenter li;

	//Do setup stuff
	Toolchain* chain = PrepBuild(rxm.toolchain());
	if(!chain)
		return;
	chdir(g_builddir.c_str());

	//Look up the list of sources
	map<string, string> sources;				//Map of fname to hash
	map<string, string> deps;
	for(int i=0; i<rxm.sources_size(); i++)
	{
		auto src = rxm.sources(i);
		sources[src.fname()] = src.hash();
	}
	for(int i=0; i<rxm.deps_size(); i++)
	{
		auto src = rxm.deps(i);
		deps[src.fname()] = src.hash();
	}

	//Get each source file
	set<string> fnames;
	for(auto it : sources)
	{
		string fname = it.first;
		//LogDebug("source %s\n", fname.c_str());
		if(!GrabSourceFile(sock, fname, it.second))
			return;
		fnames.emplace(g_builddir + "/" + fname);
	}
	for(auto it : deps)
	{
		string fname = it.first;

		//Don't grab the dep again if it's also a source
		if(sources.find(fname) != sources.end())
			continue;

		if(!GrabSourceFile(sock, fname, it.second))
			return;
	}

	//Look up the list of flags
	set<BuildFlag> flags;
	for(int i=0; i<rxm.flags_size(); i++)
	{
		string flag = rxm.flags(i);
		//LogDebug("Flag: %s\n", flag.c_str());
		flags.emplace(BuildFlag(flag));
	}

	//Format the return message
	SplashMsg reply;
	auto replym = reply.mutable_nodebuildresults();

	//Do the actual build
	string stdout;
	map<string, string> outputs;
	if(!chain->Build(
		rxm.arch(),
		fnames,
		GetBasenameOfFile(rxm.fname()),
		flags,
		outputs,
		stdout))
	{
		//Return the error code
		replym->set_success(false);
	}
	else
		replym->set_success(true);

	//Do some cleanup on the stdout:
	//* Remove blank lines and whitespace at the end
	//* Replace absolute paths to our build dir with "/"
	while (isspace(stdout[stdout.length()-1]))
		stdout.resize(stdout.length() - 1);
	stdout = str_replace(g_builddir, "", stdout);

	//Set other flags
	replym->set_stdout(stdout);
	replym->set_fname(rxm.fname());

	//Add our outputs
	for(auto it : outputs)
	{
		auto bf = replym->add_outputs();
		//LogDebug("Output file %s\n", it.first.c_str());
		bf->set_fname(it.first);
		bf->set_hash(it.second);
		bf->set_data(GetFileContents(it.first));
	}

	//Successful completion of the run, send the results to the server
	if(!replym->success())
		LogDebug("Build failed\n");
	//else
	//	LogDebug("Build complete\n");
	SendMessage(sock, reply);
}

void ShowVersion()
{
	printf(
		"SPLASH build server daemon by Andrew D. Zonenberg.\n"
		"\n"
		"License: 3-clause BSD\n"
		"This is free software: you are free to change and redistribute it.\n"
		"There is NO WARRANTY, to the extent permitted by law.\n");
}

void ShowUsage()
{
	printf("Usage: splashbuild ctlserver\n");
	exit(0);
}
