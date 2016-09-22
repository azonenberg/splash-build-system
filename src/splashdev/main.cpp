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
#include <splashcore/SplashNet.pb.h>
#include <ext/stdio_filebuf.h>

using namespace std;

void ShowUsage();
void ShowVersion();

//Map of watch descriptors to directory names
map<int, string> g_watchMap;

/**
	@brief Program entry point
 */
int main(int argc, char* argv[])
{
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

		//Bad argument
		else
		{
			ShowUsage();
			return 1;
		}

	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new STDLogSink(console_verbosity));

	//Print header
	if(console_verbosity >= Severity::NOTICE)
	{
		ShowVersion();
		printf("\n");
	}

	//Load the configuration so we know where the server is, etc
	g_clientSettings = new ClientSettings;

	//Connect to the server
	Socket sock(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if(!ConnectToServer(sock, ClientHello::CLIENT_DEVELOPER))
		return 1;

	//Send the devInfo
	SplashMsg devi;
	auto devim = devi.mutable_devinfo();
	devim->set_arch(ShellCommand("dpkg-architecture -l | grep DEB_HOST_GNU_TYPE | cut -d '=' -f 2", true));
	if(!SendMessage(sock, devi))
		return 1;

	//Recursively send file-changed notifications for everything in our working directory
	//(in case anything changed while we weren't running)
	LogVerbose("Sending initial change notifications...\n");
	SendChangeNotificationForDir(sock, g_clientSettings->GetProjectRoot());

	//Open the source directory and start an inotify watcher on it and all subdirectories
	int hnotify = inotify_init();
	if(hnotify < 0)
		LogFatal("Couldn't start inotify\n");
	LogNotice("Watching for changes to source files in: %s\n", g_clientSettings->GetProjectRoot().c_str());
	WatchDirRecursively(hnotify, g_clientSettings->GetProjectRoot());

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
				WatchedFileChanged(sock, evt->mask, g_watchMap[evt->wd] + "/" + evt->name);

			//Go on to the next one
			offset += sizeof(inotify_event) + evt->len;
		}
	}

	//Done
	close(hnotify);
	delete g_clientSettings;
	return 0;
}

/**
	@brief Add watches to a directory *and* all subdirectories
 */
void WatchDirRecursively(int hnotify, string dir)
{
	//LogDebug("    Recursively watching directory %s\n", dir.c_str());

	//Watch changes to the directory
	int wd;
	if(0 > (wd = inotify_add_watch(
		hnotify,
		dir.c_str(),
		IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF)))
	{
		LogFatal("Failed to watch directory %s\n", dir.c_str());
	}

	g_watchMap[wd] = dir;

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
	printf("Usage: splashdev [standard log arguments]\n");
	exit(0);
}
