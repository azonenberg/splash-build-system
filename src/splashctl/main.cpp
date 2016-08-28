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

#include "splashctl.h"

using namespace std;

void ShowUsage();
void ShowVersion();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Global state: toolchains

//Mutex to control access to all global node lists
mutex g_toolchainListMutex;

//TODO: start using client IDs for this instead

//List of nodes (eliminate multiple splashbuild instances)
unordered_set<string> g_activeClients;

//List of compilers available on each node
//This is the authoritative pointer to nodes
map<clientID, vtool> g_toolchainsByNode;

//List of nodes with any compiler for a given language and target architecture
map<larch, vnode> g_nodesByLanguage;

//List of nodes with a specific compiler (by hash)
map<string, vnode> g_nodesByCompiler;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Global state: files on clients

/*
	Set of hashes for source/object files we have in the cache (need to read at app startup)

	Directory structure:
	$CACHE/
		xx/				first octet of hash, as hex
			hash/		hash of file object (may not actually be the hash of the file, includes flags etc)
				xx		the file itself (original filename)
				.hash	sha256 of the file itself (for load-time integrity checking)
				.atime	last-accessed time of the file
						We don't use filesystem atime as that's way too easy to set by accident

	Global cache:
	unordered_set<string> 	listing which hashes we have in the cache
	map<string, time_t>		mapping hashes to last-used times

	Client state:
		
 */

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// App code

/**
	@brief Program entry point
 */
int main(int argc, char* argv[])
{
	int port = 49000;

	Severity console_verbosity = Severity::NOTICE;
	
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
		
		//Last arg without a switch is the port number
		//TODO: mandatory arguments to introduce this?
		//TODO: sanity check that it's numeric
		else
			port = atoi(argv[i]);

	}
	
	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new STDLogSink(console_verbosity));

	//Print header
	if(console_verbosity >= Severity::NOTICE)
	{
		ShowVersion();
		printf("\n");
	}

	//Socket server
	Socket server(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(!server.Bind(port))
		return -1;
	if(!server.Listen())
		return -1;
	while(true)
	{
		thread t(ClientThread, server.Accept().Detach());
		t.detach();
	}

	return 0;
}

void ShowVersion()
{
	printf(
		"SPLASH control server daemon by Andrew D. Zonenberg.\n"
		"\n"
		"License: 3-clause BSD\n"
		"This is free software: you are free to change and redistribute it.\n"
		"There is NO WARRANTY, to the extent permitted by law.\n");
}

void ShowUsage()
{
	printf("Usage: splashctl [control_port]\n");
	exit(0);
}
