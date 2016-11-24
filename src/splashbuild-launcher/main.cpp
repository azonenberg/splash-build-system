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

#include "splashbuild-launcher.h"

using namespace std;

void ShowUsage();
void ShowVersion();

/**
	@brief Program entry point
 */
int main(int argc, char* argv[])
{
	Severity console_verbosity = Severity::NOTICE;

	string ctl_server;
	int port = 49000;
	int nodenum = 0;

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

		//Last arg without switch is control server.
		//TODO: mandatory arguments to introduce these?
		else
			ctl_server = argv[i];

	}
	/*
	char sworker[64];
	snprintf(sworker, sizeof(sworker), "splashbuild-%d", nodenum);

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
	g_clientSettings = new ClientSettings(ctl_server, port);

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
		madd->set_exesuffix(t->GetExecutableSuffix());
		madd->set_shlibsuffix(t->GetSharedLibrarySuffix());
		madd->set_stlibsuffix(t->GetStaticLibrarySuffix());
		madd->set_objsuffix(t->GetObjectSuffix());
		madd->set_shlibprefix(t->GetSharedLibraryPrefix());
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
	*/

	return 0;
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
