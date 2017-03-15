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

#include "splash.h"
#include <splashcore/SplashNet.pb.h>
#include <ext/stdio_filebuf.h>

using namespace std;

void ShowUsage();
void ShowVersion();

//Project root directory
string g_rootDir;

//Map of watch descriptors to directory names
map<int, string> g_watchMap;

int ProcessInitCommand(const vector<string>& args);

int ProcessBuildCommand(Socket& s, const vector<string>& args);

int ProcessListArchesCommand(Socket& s, const vector<string>& args);
int ProcessListClientsCommand(Socket& s, const vector<string>& args);
int ProcessListConfigsCommand(Socket& s, const vector<string>& args);
int ProcessListTargetsCommand(Socket& s, const vector<string>& args, bool pretty);
int ProcessListToolchainsCommand(Socket& s, const vector<string>& args);
int ProcessDumpGraphCommand(Socket& s, const vector<string>& args, bool suppressSystemIncludes = true);

string GetShortHash(string hash);

/**
	@brief Program entry point
 */
int main(int argc, char* argv[])
{
	Severity console_verbosity = Severity::NOTICE;

	//Parse command-line arguments
	string cmd = "";
	vector<string> args;
	bool nobanner = false;
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

		else if(s == "--nobanner")
			nobanner = true;

		//Whatever it is, it's a command (TODO handle args)
		else if(cmd.empty())
			cmd = s;

		else
			args.push_back(s);
	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	//Print header
	if( (console_verbosity >= Severity::NOTICE) && !nobanner )
	{
		ShowVersion();
		printf("\n");
	}

	//Initialize the cache
	//TODO: Separate caches for each instance if we multithread? Or one + sharing?
	g_cache = new Cache("splash");

	//If the command is "init" we have to process it BEFORE loading the config or connecting to the server
	//since the config doesn't yet exist!
	if(cmd == "init")
		return ProcessInitCommand(args);

	//Load the configuration so we know where the server is, etc
	g_clientSettings = new ClientSettings;

	//Connect to the server
	Socket sock(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if(!ConnectToServer(sock, ClientHello::CLIENT_UI))
		return 1;

	//Send the devInfo
	SplashMsg devi;
	auto devim = devi.mutable_devinfo();
	devim->set_arch(GetTargetTriplet());
	if(!SendMessage(sock, devi))
		return 1;

	//Process other commands once the link is up and running
	if(cmd == "build")
		return ProcessBuildCommand(sock, args);
	else if(cmd == "dump-graph")
		return ProcessDumpGraphCommand(sock, args);
	else if(cmd == "list-arches")
		return ProcessListArchesCommand(sock, args);
	else if(cmd == "list-clients")
		return ProcessListClientsCommand(sock, args);
	else if(cmd == "list-configs")
		return ProcessListConfigsCommand(sock, args);
	else if(cmd == "list-targets")
		return ProcessListTargetsCommand(sock, args, true);
	else if(cmd == "list-targets-simple")
		return ProcessListTargetsCommand(sock, args, false);
	else if(cmd == "list-toolchains")
		return ProcessListToolchainsCommand(sock, args);
	else
		LogError("Unknown command \"%s\"\n", cmd.c_str());

	//Clean up and finish
	delete g_cache;
	delete g_clientSettings;
	return 0;
}

/**
	@brief Handles "splash build"
 */
int ProcessBuildCommand(Socket& s, const vector<string>& args)
{
	//Parse arguments
	string target;
	string config;
	string arch;

	for(size_t i=0; i<args.size(); i++)
	{
		if( (args[i] == "--config") && (i+1 < args.size()) )
			config = args[++i];

		else if( (args[i] == "--arch") && (i+1 < args.size()) )
			arch = args[++i];

		else if(target.empty())
			target = args[i];

		else
		{
			LogError("Invalid argument\n");
		}
	}

	//Go to the project root dir since everything is ref'd to that
	chdir(g_clientSettings->GetProjectRoot().c_str());

	//Format the command
	SplashMsg cmd;
	auto cmdm = cmd.mutable_buildrequest();
	cmdm->set_target(target);
	cmdm->set_arch(arch);
	cmdm->set_config(config);
	cmdm->set_rebuild(false);
	if(!SendMessage(s, cmd))
		return 1;

	//Get the compiled files back from the server
	SplashMsg msg;
	if(!RecvMessage(s, msg))
		return 1;
	if(msg.Payload_case() != SplashMsg::kBuildResults)
	{
		LogError("Got wrong message type back\n");
		return 1;
	}

	//Loop over the generated files and see where we stand
	LogNotice("\n");
	auto result = msg.buildresults();
	for(int i=0; i<result.results_size(); i++)
	{
		auto r = result.results(i);
		auto f = r.fname();
		auto h = r.idhash();
		auto c = r.contenthash();

		if(!ValidatePath(f))
		{
			LogError("Filename \"%s\" is invalid, skipping\n", f.c_str());
			continue;
		}

		//If the node failed to build, don't worry about caching or downloading the output
		if(!r.ok())
		{
			//Delete the output path since it failed to build
			unlink(f.c_str());
		}

		//Node built OK, grab the contents from somewhere (unless the file is marked no-sync)
		else if(r.sync())
		{
			//See if the file is any different from what we have locally
			string ourhash = sha256_file(f);
			if(ourhash == c)
			{
				//LogNotice("Skipping sync of file %s because it didn't change\n", f.c_str());
				continue;
			}
			else
			{
				/*
				LogNotice("Syncing file %s\n    current hash = %s\n    new hash = %s\n    idhash = %s\n",
					f.c_str(),
					ourhash.c_str(),
					c.c_str(),
					h.c_str());
				*/
			}

			//See if we have the file in our local cache.
			//If not, download it
			string edat;
			if(!g_cache->IsCached(h))
			{
				if(!GetRemoteFileByHash(s, g_clientSettings->GetServerHostname(), h, edat))
				{
					LogError("Could not get file \"%s\" (hash = \"%s\") from server\n",
						f.c_str(), h.c_str());
					continue;
				}
				g_cache->AddFile(f, h, sha256(edat), edat, "");
			}
			else
				edat = g_cache->ReadCachedFile(h);

			//Delete the file first.
			//This is ABSOLUTELY CRITICAL when writing to a .so or executable that might currently be
			//running memory mapped! Deleting and re-creating forces us to get a new inode number and thus
			//makes the new binary independent of the running app's mapping.
			if(DoesFileExist(f))
			{
				if(0 != unlink(f.c_str()))
					LogWarning("Tried to delete file %s before replacing it, but failed\n", f.c_str());
			}

			//Make the directory if needed, then write the file
			string path = GetDirOfFile(f);
			MakeDirectoryRecursive(path, 0700);
			//LogDebug("Downloading file %s\n", f.c_str());
			if(!PutFileContents(f, edat))
			{
				LogError("Failed to write file \"%s\"\n", f.c_str());
				continue;
			}

			//Set the file executable if we need to
			if(r.executable())
				chmod(f.c_str(), 0700);
			else
				chmod(f.c_str(), 0600);
		}

		//If stdout is empty, don't print the file name (needless clutter)
		string stdout = r.stdout();
		if(stdout == "")
			continue;

		LogNotice("%s:\n", f.c_str());
		LogIndenter li;
		LogNotice("%s\n", stdout.c_str());
	}

	//Print overall status
	if(!result.status())
	{
		LogNotice("\nBuild failed\n");
		return 1;
	}
	else
	{
		LogNotice("\nBuild complete\n");
		return 0;
	}
}

/**
	@brief Handles "splash init"
 */
int ProcessInitCommand(const vector<string>& args)
{
	//Sanity check
	if(args.size() < 1)
	{
		LogError("Missing arguments. Usage:  \"splash init <control server> [control server port]\"\n");
		return 1;
	}

	//Assume the current directory is the working copy root
	string dir = CanonicalizePath(".");
	LogNotice("Initializing working copy at %s\n", dir.c_str());

	//Make the config directory
	string splashdir = dir  + "/.splash";
	MakeDirectoryRecursive(splashdir, 0700);

	//Parse the args
	string server = args[0];	//TODO: force to be valid hostname
	string port = "49000";
	if(args.size() > 1)
		port = args[1];			//TODO: force to be integer

	//Write the final config.yml
	string cfgpath = splashdir + "/config.yml";
	string config = "server:\n";
	config +=		"    host: " + server + "\n";
	config +=		"    port: " + port + "\n";
	config +=       "\n";
	config +=		"client:\n";
	config +=		"    uuid: \"" + ShellCommand("uuidgen -r") + "\"\n";
	PutFileContents(cfgpath, config);

	//We're good
	return 0;
}

/**
	@brief Handles "splash list-toolchains"
 */
int ProcessListToolchainsCommand(Socket& s, const vector<string>& args)
{
	//Sanity check
	if(args.size() != 0)
	{
		LogError("Extra arguments. Usage:  \"splash list-toolchains\"\n");
		return 1;
	}

	//Format the command
	SplashMsg cmd;
	auto cmdm = cmd.mutable_inforequest();
	cmdm->set_type(InfoRequest::TOOLCHAIN_LIST);
	if(!SendMessage(s, cmd))
		return 1;

	//Get the response back
	SplashMsg msg;
	if(!RecvMessage(s, msg))
		return 1;
	if(msg.Payload_case() != SplashMsg::kToolchainList)
	{
		LogError("Got wrong message type back\n");
		return 1;
	}

	auto lt = msg.toolchainlist();
	LogNotice("%-20s %-15s %-15s %-25s %-25s %-30s\n", "Version", "Hash", "Languages", "Name", "Architectures", "Nodes");
	for(int i=0; i<lt.infos_size(); i++)
	{
		auto info = lt.infos(i);

		//Figure out how many rows we need
		int nnames = info.names_size();
		int nclients = info.uuids_size();
		int nlangs = info.langs_size();
		int narches = info.arches_size();
		int nmax = max(nnames, nclients);
		nmax = max(nmax, nlangs);
		nmax = max(nmax, narches);

		//Print each row
		LogNotice(
			"---------------------------------------------------------------------"
			"---------------------------------------------------\n");
		for(int j=0; j<nmax; j++)
		{
			//Truncate hash for display
			string hash;
			if(j == 0)
				hash = GetShortHash(info.hash());

			string version;
			if(j == 0)
				version = info.version();

			string name;
			if(j < nnames)
				name = info.names(j);

			string lang;
			if(j < nlangs)
				lang = info.langs(j);

			string arch;
			if(j < narches)
				arch = info.arches(j);

			string uuid;
			if(j < nclients)
				uuid = info.uuids(j);

			LogNotice(
				"%-20s %-15s %-15s %-25s %-25s %-30s\n",
				version.c_str(),
				hash.c_str(),
				lang.c_str(),
				name.c_str(),
				arch.c_str(),
				uuid.c_str());
		}
	}

	//all good
	return 0;
}

/**
	@brief Handles "splash list-targets"
 */
int ProcessListTargetsCommand(Socket& s, const vector<string>& args, bool pretty)
{
	//Sanity check
	if(args.size() != 0)
	{
		LogError("Extra arguments. Usage:  \"splash list-targets\"\n");
		return 1;
	}

	//Format the command
	SplashMsg cmd;
	auto cmdm = cmd.mutable_inforequest();
	cmdm->set_type(InfoRequest::TARGET_LIST);
	if(!SendMessage(s, cmd))
		return 1;

	//Get the response back
	SplashMsg msg;
	if(!RecvMessage(s, msg))
		return 1;
	if(msg.Payload_case() != SplashMsg::kTargetList)
	{
		LogError("Got wrong message type back\n");
		return 1;
	}

	//Pretty-print
	if(pretty)
	{
		auto lt = msg.targetlist();
		LogNotice("%-30s %-15s %-30s\n", "Target", "Toolchain", "Script");
		for(int i=0; i<lt.info_size(); i++)
		{
			auto info = lt.info(i);
			LogNotice(
				"%-30s %-15s %-20s\n",
				info.name().c_str(),
				info.toolchain().c_str(),
				info.script().c_str());
		}
	}

	//Simple print
	else
	{
		auto lt = msg.targetlist();
		for(int i=0; i<lt.info_size(); i++)
			LogNotice("%s\n", lt.info(i).name().c_str());
	}

	//all good
	return 0;
}

/**
	@brief Handles "splash list-configs"
 */
int ProcessListConfigsCommand(Socket& s, const vector<string>& args)
{
	//Sanity check
	if(args.size() != 0)
	{
		LogError("Extra arguments. Usage:  \"splash list-configs\"\n");
		return 1;
	}

	//TODO: do "list configs for target"

	//Format the command
	SplashMsg cmd;
	auto cmdm = cmd.mutable_inforequest();
	cmdm->set_type(InfoRequest::CONFIG_LIST);
	if(!SendMessage(s, cmd))
		return 1;

	//Get the response back
	SplashMsg msg;
	if(!RecvMessage(s, msg))
		return 1;
	if(msg.Payload_case() != SplashMsg::kConfigList)
	{
		LogError("Got wrong message type back\n");
		return 1;
	}

	//and process it
	auto lt = msg.configlist();
	for(int i=0; i<lt.configs_size(); i++)
		LogNotice("%s\n", lt.configs(i).c_str());

	//all good
	return 0;
}


/**
	@brief Handles "splash list-configs"
 */
int ProcessListClientsCommand(Socket& s, const vector<string>& args)
{
	//Sanity check
	if(args.size() != 0)
	{
		LogError("Extra arguments. Usage:  \"splash list-clients\"\n");
		return 1;
	}

	//Format the command
	SplashMsg cmd;
	auto cmdm = cmd.mutable_inforequest();
	cmdm->set_type(InfoRequest::CLIENT_LIST);
	if(!SendMessage(s, cmd))
		return 1;

	//Get the response back
	SplashMsg msg;
	if(!RecvMessage(s, msg))
		return 1;
	if(msg.Payload_case() != SplashMsg::kClientList)
	{
		LogError("Got wrong message type back\n");
		return 1;
	}

	auto lt = msg.clientlist();
	LogNotice("%-15s %-20s %-30s\n", "Type", "Hostname", "UUID");
	for(int i=0; i<lt.infos_size(); i++)
	{
		auto info = lt.infos(i);

		string stype;
		switch(info.type())
		{
			case ClientHello::CLIENT_DEVELOPER:
				stype = "splashdev";
				break;

			case ClientHello::CLIENT_BUILD:
				stype = "splashbuild";
				break;

			case ClientHello::CLIENT_UI:
				stype = "splash";
				break;

			default:
				stype = "<error>";
				break;
		}

		LogNotice(
			"%-15s %-20s %-30s\n",
			stype.c_str(),
			info.hostname().c_str(),
			info.uuid().c_str());
	}

	//all good
	return 0;
}

/**
	@brief Handles "splash list-arches"
 */
int ProcessListArchesCommand(Socket& s, const vector<string>& args)
{
	//Sanity check
	if(args.size() > 1)
	{
		LogError("Extra arguments. Usage:  \"splash list-arches [target]\"\n");
		return 1;
	}

	//Format the command
	SplashMsg cmd;
	auto cmdm = cmd.mutable_inforequest();
	cmdm->set_type(InfoRequest::ARCH_LIST);
	if(args.size() == 1)
		cmdm->set_query(args[0]);
	if(!SendMessage(s, cmd))
		return 1;

	//Get the response back
	SplashMsg msg;
	if(!RecvMessage(s, msg))
		return 1;
	if(msg.Payload_case() != SplashMsg::kArchList)
	{
		LogError("Got wrong message type back\n");
		return 1;
	}

	//and process it
	auto lt = msg.archlist();
	for(int i=0; i<lt.arches_size(); i++)
		LogNotice("%s\n", lt.arches(i).c_str());

	//all good
	return 0;
}

/**
	@brief Handles "splash dump-graph"
 */
int ProcessDumpGraphCommand(Socket& s, const vector<string>& args, bool suppressSystemIncludes)
{
	//Sanity check
	if(args.size() != 0)
	{
		LogError("Extra arguments. Usage:  \"splash list-clients\"\n");
		return 1;
	}

	//Format the command
	SplashMsg cmd;
	auto cmdm = cmd.mutable_inforequest();
	cmdm->set_type(InfoRequest::NODE_LIST);
	if(!SendMessage(s, cmd))
		return 1;

	//Get the response back
	SplashMsg msg;
	if(!RecvMessage(s, msg))
		return 1;
	if(msg.Payload_case() != SplashMsg::kNodeList)
	{
		LogError("Got wrong message type back\n");
		return 1;
	}

	LogNotice("digraph build\n{\n");
	LogNotice("rankdir = LR;\n");
	LogNotice("dpi = 200;\n");

	//Make a list of nodes being hidden
	auto lt = msg.nodelist();
	set<string> hiddenNodes;
	if(suppressSystemIncludes)
	{
		for(int i=0; i<lt.infos_size(); i++)
		{
			auto node = lt.infos(i);
			auto path = node.path();
			auto hash = node.hash();

			//See if we're hiding this node
			if(path.find("__sysinclude__") != string::npos)
				hiddenNodes.emplace(hash);
		}
	}

	//and process it
	for(int i=0; i<lt.infos_size(); i++)
	{
		auto node = lt.infos(i);
		auto path = node.path();
		auto rawhash = node.hash();
		auto hash = GetShortHash(rawhash);

		//If we're hiding the node, don't even show it
		if(hiddenNodes.find(rawhash) != hiddenNodes.end())
			continue;

		string state = "invalid";
		switch(node.state())
		{
			case NodeInfo::READY:
				state = "ready";
				break;

			case NodeInfo::BUILDING:
				state = "building";
				break;

			case NodeInfo::MISSING:
				state = "missing";
				break;

			default:
				break;
		}

		//Dump the node name info
		string label = "<table cellspacing=\"0\">\n";
		label += string("<tr><td><b>Path</b></td><td>") + path + "</td></tr>\n";
		label += string("<tr><td><b>Hash</b></td><td>") + node.hash() + "</td></tr>\n";
		label += string("<tr><td><b>State</b></td><td>") + state + "</td></tr>\n";
		label += string("<tr><td><b>Arch</b></td><td>") + node.arch() + "</td></tr>\n";
		if(node.script() != "")
			label += string("<tr><td><b>Script</b></td><td>") + node.script() + "</td></tr>\n";
		if(node.toolchain() != "")
			label += string("<tr><td><b>Toolchain</b></td><td>") + node.toolchain() + "</td></tr>\n";
		label += string("<tr><td><b>Config</b></td><td>") + node.config() + "</td></tr>\n";
		label += "</table>\n";

		//Create the actual node
		LogNotice("\"%s\" [shape=none, margin=0, label=<%s>];\n", hash.c_str(), label.c_str());

		//Dump our edges
		for(int j=0; j<node.deps_size(); j++)
		{
			string dephash = node.deps(j);

			//If it points to a hidden node, don't show it
			if(hiddenNodes.find(dephash) != hiddenNodes.end())
				continue;

			LogNotice("\"%s\" -> \"%s\";\n", hash.c_str(), GetShortHash(dephash).c_str());
		}
	}

	LogNotice("}\n");

	//all good
	return 0;
}

void ShowVersion()
{
	printf(
		"SPLASH workstation client by Andrew D. Zonenberg.\n"
		"\n"
		"License: 3-clause BSD\n"
		"This is free software: you are free to change and redistribute it.\n"
		"There is NO WARRANTY, to the extent permitted by law.\n");
}

void ShowUsage()
{
	printf(
		"Usage: splash [options] <command> [args]\n"
		"\n"
		"options may be zero or more of the following:\n"
		"    --debug                        Sets log level to debug\n"
		"    -l, --logfile <fname>          Directs logging to the file <fname>\n"
		"    -L, --logfile-lines <fname>    Directs logging to the file <fname> and\n"
		"                                   turns on line buffering\n"
		"    --nobanner                     Do not print header/license at startup\n"
		"    -q, --quiet                    Decreases log verbosity by one step\n"
		"    --verbose                      Sets log level to verbose\n"
		"\n"
		"command may be one of the following:\n"
		"    build                          Builds one or more targets\n"
		"    dump-graph                     Print the entire dependency graph to stdout\n"
		"                                   in graphviz format.\n"
		"    init                           Initialize a new working copy\n"
		"    list-arches [target]           List all architectures the specified target is\n"
		"                                   to be compiled for. With no argument, list all\n"
		"                                   architectures that we have any target for.\n"
		"    list-clients                   List all clients connected to the server.\n"
		"    list-configs                   List all configurations we have at least\n"
		"                                   one target for.\n"
		"    list-targets                   List all targets in the working copy,\n"
		"                                   pretty printed with details\n"
		"    list-targets-simple            List all targets in the working copy,\n"
		"                                   with one target name per line\n"
		"    list-toolchains                List all toolchains the server knows about.\n"
		"\n"
		"splash build \n"
		"    Builds one or more targets (docs TODO)\n"
		"\n"
		"splash init <control server> [port]\n"
		"    Initializes the .splash directory within this working copy to store\n"
		"    client-side configuration. This must be the first Splash command\n"
		"    executed in the working copy after cloning.\n"
		"\n"
		"    control server: Hostname of the splashctl server. Required.\n"
		"    port: Port number of the splashctl server. Defaults to 49000.\n"
	);
	exit(0);
}

string GetShortHash(string hash)
{
	return string("...") + hash.substr(55);
}
