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

void sig_handler(int sig);

void ShowUsage();
void ShowVersion();

bool g_quitting = false;
Socket* g_server;

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
		else
		{
			int tmp = atoi(argv[i]);

			//Sanity check
			if(tmp == 0)
			{
				fprintf(
					stderr,
					"ERROR: Don't know what argument \"%s\" is supposed to mean, but it's not a valid port number\n",
					argv[i]);
				return 0;
			}
			else
				port = tmp;
		}

	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	//Print header
	if(console_verbosity >= Severity::NOTICE)
	{
		ShowVersion();
		printf("\n");
	}

	//Install signal handler
	signal(SIGINT, sig_handler);
	signal(SIGPIPE, sig_handler);

	//Initialize global data structures
	g_cache = new Cache("splashctl");
	g_nodeManager = new NodeManager;
	g_scheduler = new Scheduler;

	//Socket server
	LogDebug("Listening on TCP port %d...\n", port);
	Socket server(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	g_server = &server;
	if(!server.DisableNagle())
		return -1;
	if(!server.Bind(port))
		return -1;
	if(!server.Listen())
		return -1;
	while(true)
	{
		auto client = server.Accept();
		if(!client.IsValid())
			break;
		thread t(ClientThread, client.Detach());
		t.detach();
	}

	//Cleanup
	delete g_nodeManager;
	delete g_scheduler;
	delete g_cache;
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

void sig_handler(int sig)
{
	switch(sig)
	{
		case SIGINT:
			printf("Quitting...\n");

			g_quitting = true;
			close(g_server->Detach());

			break;

		case SIGPIPE:
			//ignore
			break;
	}
}
