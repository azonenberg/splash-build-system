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

/**
	@brief Program entry point
 */
int main(int argc, char* argv[])
{
	string ctl_server;

	Severity console_verbosity = Severity::NOTICE;

	//TODO: argument for this?
	int port = 49000;

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

		//Last two args without switches are source dir and control server.
		//TODO: mandatory arguments to introduce these?
		else if(g_rootDir == "")
			g_rootDir = argv[i];

		else
			ctl_server = argv[i];

	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new STDLogSink(console_verbosity));

	//Print header
	if(console_verbosity >= Severity::NOTICE)
	{
		ShowVersion();
		printf("\n");
	}

	//Connect to the server
	LogVerbose("Connecting to server...\n");
	Socket sock(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	sock.Connect(ctl_server, port);

	//Get the serverHello
	SplashMsg shi;
	if(!RecvMessage(sock, shi, ctl_server))
		return 1;
	if(shi.Payload_case() != SplashMsg::kServerHello)
	{
		LogWarning("Connection dropped (expected serverHello, got %d instead)\n",
			shi.Payload_case());
		return 1;
	}
	auto shim = shi.serverhello();
	if(shim.magic() != SPLASH_PROTO_MAGIC)
	{
		LogWarning("Connection dropped (bad magic number in serverHello)\n");
		return 1;
	}
	if(shim.version() != SPLASH_PROTO_VERSION)
	{
		LogWarning("Connection dropped (bad version number in serverHello)\n");
		return 1;
	}

	//Send the clientHello
	SplashMsg chi;
	auto chim = chi.mutable_clienthello();
	chim->set_magic(SPLASH_PROTO_MAGIC);
	chim->set_version(SPLASH_PROTO_VERSION);
	chim->set_type(ClientHello::CLIENT_UI);
	chim->set_hostname(ShellCommand("hostname", true));
	if(!SendMessage(sock, chi, ctl_server))
		return 1;

	//Send the devInfo
	SplashMsg devi;
	auto devim = devi.mutable_devinfo();
	devim->set_arch(ShellCommand("dpkg-architecture -l | grep DEB_HOST_GNU_TYPE | cut -d '=' -f 2", true));
	if(!SendMessage(sock, devi, ctl_server))
		return 1;

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
	printf("Usage: splash ARG_TODO\n");
	exit(0);
}
