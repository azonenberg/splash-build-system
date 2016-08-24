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

/**
	@brief Program entry point
 */
int main(int argc, char* argv[])
{
	string source_dir;
	string ctl_server;
	
	LogSink::Severity console_verbosity = LogSink::NOTICE;
	
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
		else if(source_dir == "")				
			source_dir = argv[i];
		
		else	
			ctl_server = argv[i];

	}
	
	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new STDLogSink(console_verbosity));

	//Print header
	if(console_verbosity >= LogSink::NOTICE)
	{
		ShowVersion();
		printf("\n");
	}
	
	//Connect to the server
	LogVerbose("Connecting to server...\n");
	Socket sock(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sock.Connect(ctl_server, port);
	
	//Get the serverHello
	msgServerHello shi;
	if(!sock.RecvLooped((unsigned char*)&shi, sizeof(shi)))
	{
		LogWarning("Connection dropped (while reading serverHello)\n");
		return 1;
	}
	if(shi.serverVersion != 1)
	{
		LogWarning("Connection dropped (bad version number in serverHello)\n");
		return 1;
	}
	if(shi.type != MSG_TYPE_SERVERHELLO)
	{
		LogWarning("Connection dropped (bad message type in serverHello)\n");
		return 1;
	}
	
	//Send the clientHello
	msgClientHello chi(CLIENT_BUILD);
	if(!sock.SendLooped((unsigned char*)&chi, sizeof(chi)))
	{
		LogWarning("Connection dropped (while sending clientHello)\n");
		return 1;
	}
	string hostname = ShellCommand("hostname", true);
	if(!sock.SendPascalString(hostname))
	{
		LogWarning("Connection dropped (while sending clientHello.hostname)\n");
		return 1;
	}
	
	//Validate the connection
	if(chi.magic != shi.magic)
	{
		LogWarning("Connection dropped (bad magic number in serverHello)\n");
		return 1;
	}
	
	//Get some basic metadata about our hardware and tell the server
	msgBuildInfo binfo;
	binfo.cpuCount = atoi(ShellCommand("cat /proc/cpuinfo  | grep processor | wc -l").c_str());
	binfo.cpuSpeed = atoi(ShellCommand("cat /proc/cpuinfo | grep bogo | head -n 1 | cut -d : -f 2").c_str());
	binfo.ramSize = atol(ShellCommand("cat /proc/meminfo  | grep MemTotal  | cut -d : -f 2").c_str()) / 1024;
	if(!sock.SendLooped((unsigned char*)&binfo, sizeof(binfo)))
	{
		LogWarning("Connection dropped (while sending buildInfo)\n");
		return 1;
	}
	
	//Look for compilers
	LogVerbose("Enumerating compilers...\n");
	FindLinkers();
	FindCPPCompilers();
	FindFPGACompilers();
	
	//Report the toolchains we found to the server
	
	//TODO: Sit around and wait for stuff to come in
	
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
	printf("Usage: splashbuild tempdir ctlserver\n");
	exit(0);
}
