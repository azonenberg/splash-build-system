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

bool ProcessBulkFileAck(Socket& s, SplashMsg& msg)
{
	//Sanity check
	if(msg.Payload_case() != SplashMsg::kBulkFileAck)
	{
		LogWarning("Invalid message (expected bulkFileAck, got %d instead)\n",
			msg.Payload_case());
		return false;
	}
	auto ack = msg.bulkfileack();

	//Prepare the response message
	SplashMsg cont;
	auto contm = cont.mutable_bulkfiledata();
	bool empty = true;

	//Look at the response and see what's what
	for(int i=0; i<ack.acks_size(); i++)
	{
		//Skip the file if it's already in cache server side
		auto a = ack.acks(i);
		if(a.filecached())
			continue;

		//Server does NOT have it in cache! Send the content
		//But first, make sure the file name is valid
		string fname = a.fname();
		if(!ValidatePath(fname))
		{
			LogError("File name \"%s\" is invalid (contains .. or leading /)\n", fname.c_str());
			return false;
		}

		//LogDebug("new content for %s is not in cache, sending file to server\n", fname.c_str());

		//Send the stuff
		empty = false;
		auto c = contm->add_data();
		string data = GetFileContents(fname);
		string hash = sha256(data);
		c->set_fname(fname);
		c->set_hash(hash);
		c->set_id(hash);
		c->set_filedata(data);
	}

	//If we have at least one file worth of content to send, do so
	if(!empty)
	{
		if(!SendMessage(s, cont, "server"))
			return false;
	}

	return true;
}

/**
	@brief Makes a change notification message for a given directory

	@param s			Server socket
	@param path			Path to the directory that changed
 */
void BuildChangeNotificationForDir(BulkFileChanged* msg, string path)
{
	//Stop if it doesn't exist (sanity check)
	if(!DoesDirectoryExist(path))
		return;

	//If the directory has a ".splashignore" file in it, don't do anything
	if(DoesFileExist(path  + "/.splashignore"))
		return;

	//LogDebug("Sending change notification for %s\n", path.c_str());

	//See if we have a build.yml in this directory
	//If so, pre-send its config section (since it may contain config inherited by our children)
	string scriptpath = path + "/build.yml";
	bool hasScript = DoesFileExist(scriptpath);
	//if(hasScript)
	//	BuildChangeNotificationForFile(msg, scriptpath, false, true);

	//Send change notices for our subdirectories
	vector<string> children;
	FindSubdirs(path, children);
	for(auto x : children)
		BuildChangeNotificationForDir(msg, x);

	//Send change notices for our files
	//(except for the build script, if present).
	//Do not recursively re-scan the build script as we're deferring that for later
	vector<string> files;
	FindFiles(path, files);
	for(auto x : files)
	{
		if(x != scriptpath)
			BuildChangeNotificationForFile(msg, x, true, false);
	}

	//Finally, load the body of the build script
	//(but not the config, since that would require re-parsing our children)
	if(hasScript)
	{
		//	BuildChangeNotificationForFile(msg, scriptpath, true, false);
		BuildChangeNotificationForFile(msg, scriptpath, true, true);
	}
}

/**
	@brief Builds a change notification for a single file

	@param msg			Server socket
	@param path			Path to the file that changed
	@param body			True if we should re-parse the body (only meaningful if path is a build.yml)
	@param config		True if we should re-parse the config section (only meaningful if path is a build.yml)
 */
void BuildChangeNotificationForFile(BulkFileChanged* msg, string path, bool body, bool config)
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

	//LogDebug("Sending change notification for %s\n", path.c_str());

	//Tell the server it changed
	auto mcg = msg->add_files();
	mcg->set_fname(fname);
	mcg->set_hash(hash);
	mcg->set_body(body);
	mcg->set_config(config);
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
