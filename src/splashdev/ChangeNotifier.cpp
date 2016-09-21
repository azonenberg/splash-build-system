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

/**
	@brief Sends change notifications to the server for a given directory

	@param s			Server socket
	@param path			Path to the directory that changed
 */
void SendChangeNotificationForDir(Socket& s, string path)
{
	//Stop if it doesn't exist (sanity check)
	if(!DoesDirectoryExist(path))
		return;

	//See if we have a build.yml in this directory
	//If so, pre-send its config section (since it may contain config inherited by our children)
	string scriptpath = path + "/build.yml";
	bool hasScript = DoesFileExist(scriptpath);
	if(hasScript)
		SendChangeNotificationForFile(s, scriptpath, false, true);

	//Send change notices for our subdirectories
	vector<string> children;
	FindSubdirs(path, children);
	for(auto x : children)
		SendChangeNotificationForDir(s, x);

	//Send change notices for our files
	//(except for the build script, if present).
	//Do not recursively re-scan the build script as we're deferring that for later
	vector<string> files;
	FindFiles(path, files);
	for(auto x : files)
	{
		if(x != scriptpath)
			SendChangeNotificationForFile(s, x, true, false);
	}

	//Finally, load the body of the build script
	//(but not the config, since that would require re-parsing our children)
	if(hasScript)
		SendChangeNotificationForFile(s, scriptpath, true, false);
}

/**
	@brief Sends a change notification to the server for a single file

	@param s			Server socket
	@param path			Path to the file that changed
	@param body			True if we should re-parse the body (only meaningful if path is a build.yml)
	@param config		True if we should re-parse the config section (only meaningful if path is a build.yml)
 */
void SendChangeNotificationForFile(Socket& s, string path, bool body, bool config)
{
	//Hash the file
	string hash = sha256_file(path);

	//Get path relative to project root
	string fname = path;
	string root = g_clientSettings->GetProjectRoot();
	if(fname.find(root) == 0)
		fname = fname.substr(root.length() + 1);	//add 1 to trim trailing /
	else
		LogWarning("Changed file %s is not within project root\n", path.c_str());

	LogDebug("Sending change notification for %s\n", path.c_str());

	//Tell the server it changed
	SplashMsg change;
	auto mcg = change.mutable_filechanged();
	mcg->set_fname(fname);
	mcg->set_hash(hash);
	mcg->set_body(body);
	mcg->set_config(config);
	if(!SendMessage(s, change, "server"))
		return;

	//Ask the server if they need content of the file
	SplashMsg ack;
	if(!RecvMessage(s, ack, "server"))
		return;
	if(ack.Payload_case() != SplashMsg::kFileAck)
	{
		LogWarning("Invalid message (expected fileAck, got %d instead)\n",
			ack.Payload_case());
		return;
	}

	//Don't send the file if the server already has it in the cache
	if(ack.fileack().filecached())
		return;

	LogDebug("    new content is not in cache, sending file to server\n");

	//Read the file into RAM
	//TODO: cap for HUGE files, don't send them to the server by default
	SplashMsg data;
	auto mdat = data.mutable_filedata();
	mdat->set_filedata(GetFileContents(path));
	if(!SendMessage(s, data, "server"))
		return;
}

void SendDeletionNotificationForFile(Socket& s, std::string path)
{
	//Get path relative to project root
	string fname = path;
	string root = g_clientSettings->GetProjectRoot();
	if(fname.find(root) == 0)
		fname = fname.substr(root.length() + 1);	//add 1 to trim trailing /
	else
		LogWarning("Deleted file %s is not within project root\n", path.c_str());

	LogDebug("Sending deletion notification for %s\n", path.c_str());

	//Tell the server it changed
	SplashMsg change;
	auto mcg = change.mutable_fileremoved();
	mcg->set_fname(fname);
	if(!SendMessage(s, change, "server"))
		return;
}
