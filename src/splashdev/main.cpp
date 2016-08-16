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

#include "../splashcore/splashcore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string>

#include <sys/inotify.h>

using namespace std;

void ShowUsage();

void WatchDirRecursively(int hnotify, string dir);

int main(int argc, char* argv[])
{
	string source_dir;
	string ctl_server;
	
	//Parse command-line arguments
	for(int i=1; i<argc; i++)
	{
		string s(argv[i]);
		
		if(s == "--help")
		{
			ShowUsage();
			return 0;
		}
		
		else if(source_dir == "")				
			source_dir = argv[i];
		
		else	
			ctl_server = argv[i];

	}
	
	//Open the source directory and start an inotify watcher on it and all subdirectories
	source_dir = CanonicalizePath(source_dir);
	int hnotify = inotify_init();
	if(hnotify < 0)
		FatalError("Couldn't start inotify\n");
	WatchDirRecursively(hnotify, source_dir);

	//TODO: signal handler so we can quit gracefully

	//Main event loop
	size_t buflen = sizeof(inotify_event) + NAME_MAX + 1;
	inotify_event* evt = (inotify_event*)malloc(buflen);
	if(evt == NULL)
		FatalError("Couldn't allocate buffer\n");
	while(1)
	{
		//Get the event
		size_t len = read(hnotify, evt, buflen);
		if(len <= 0)
			break;
			
		//See what it is
		printf("Got event of type %d for %s (len %d, namelen %d)\n", evt->mask, evt->name, len, evt->len);
	}
	
	//Done
	free(evt);
	close(hnotify);	
	return 0;
}

void WatchDirRecursively(int hnotify, string dir)
{
	//Watch changes to the directory
	if(0 > inotify_add_watch(
		hnotify,
		dir.c_str(),
		IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF))
	{
		FatalError("Failed to watch directory %s\n", dir.c_str());
	}
	
	//Look for any subdirs and watch them
	vector<string> subdirs;
	FindSubdirs(dir, subdirs);
	for(auto s : subdirs)
		WatchDirRecursively(hnotify, s);
}

void ShowUsage()
{
	printf("Usage: splashdev srcdir ctlserver\n");
	exit(0);
}
