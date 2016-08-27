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

#include "splashdev.h"

using namespace std;

void ShowUsage();
void ShowVersion();

//Project root directory
string g_rootDir;

/**
	@brief Program entry point
 */
int main(int argc, char* argv[])
{
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
		else if(g_rootDir == "")				
			g_rootDir = argv[i];
		
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
	msgClientHello chi(CLIENT_DEVELOPER);
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
	
	//Send the developer node info
	//TODO: Make this work on non-Debian systems!
	msgDevInfo dinfo;
	if(!sock.SendLooped((unsigned char*)&dinfo, sizeof(dinfo)))
	{
		LogWarning("Connection dropped (while sending devInfo)\n");
		return 1;
	}
	string arch = ShellCommand("dpkg-architecture -l | grep DEB_HOST_GNU_TYPE | cut -d '=' -f 2", true);
	if(!sock.SendPascalString(arch))
	{
		LogWarning("Connection dropped (while sending devInfo.arch)\n");
		return 1;
	}
	
	//Recursively send file-changed notifications for everything in our working directory
	//(in case anything changed while we weren't running)
	LogVerbose("Sending initial change notifications...\n");
	g_rootDir = CanonicalizePath(g_rootDir);
	SendChangeNotification(sock, g_rootDir);
	
	//Open the source directory and start an inotify watcher on it and all subdirectories
	int hnotify = inotify_init();
	if(hnotify < 0)
		LogFatal("Couldn't start inotify\n");
	LogNotice("Working copy root directory: %s\n", g_rootDir.c_str());
	WatchDirRecursively(hnotify, g_rootDir);

	//TODO: signal handler so we can quit gracefully

	//Main event loop
	size_t buflen = 8192;
	char ebuf[buflen];
	while(1)
	{
		//Get the event
		ssize_t len = read(hnotify, ebuf, buflen);
		if(len <= 0)
			break;
			
		ssize_t offset = 0;
		while(offset < len)
		{
			inotify_event* evt = reinterpret_cast<inotify_event*>(ebuf + offset);
			
			//Skip events without a filename, or hidden files
			if( (evt->len != 0) && (evt->name[0] != '.') )
				WatchedFileChanged(sock, evt->mask, CanonicalizePath(g_rootDir + "/" + evt->name));
			
			//Go on to the next one
			offset += sizeof(inotify_event) + evt->len;
		}
	}
	
	//Done
	close(hnotify);	
	return 0;
}

/**
	@brief Add watches to a directory *and* all subdirectories
 */
void WatchDirRecursively(int hnotify, string dir)
{
	LogDebug("    Recursively watching directory %s\n", dir.c_str());
	
	//Watch changes to the directory
	if(0 > inotify_add_watch(
		hnotify,
		dir.c_str(),
		IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF))
	{
		LogFatal("Failed to watch directory %s\n", dir.c_str());
	}
	
	//Look for any subdirs and watch them
	vector<string> subdirs;
	FindSubdirs(dir, subdirs);
	for(auto s : subdirs)
		WatchDirRecursively(hnotify, s);
}

void ShowVersion()
{
	printf(
		"SPLASH developer workstation daemon by Andrew D. Zonenberg.\n"
		"\n"
		"License: 3-clause BSD\n"
		"This is free software: you are free to change and redistribute it.\n"
		"There is NO WARRANTY, to the extent permitted by law.\n");
}

void ShowUsage()
{
	printf("Usage: splashdev srcdir ctlserver\n");
	exit(0);
}
