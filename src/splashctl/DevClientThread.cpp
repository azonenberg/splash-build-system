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

bool OnBulkFileData(Socket& s, const BulkFileData& msg, string& hostname, clientID id);
bool OnBulkFileChanged(Socket& s, const BulkFileChanged& msg, string& hostname, clientID id);
bool OnFileRemoved(const FileRemoved& msg, string& hostname, clientID id);

void DevClientThread(Socket& s, string& hostname, clientID id)
{
	LogNotice("Developer workstation %s (%s) connected\n", hostname.c_str(), id.c_str());
	//LogIndenter li;

	//Set thread name
	#ifdef _GNU_SOURCE
	string threadname = ("DEV:") + hostname;
	threadname.resize(15);
	pthread_setname_np(pthread_self(), threadname.c_str());
	#endif

	//Expect a DevInfo message
	SplashMsg dinfo;
	if(!RecvMessage(s, dinfo, hostname))
		return;
	if(dinfo.Payload_case() != SplashMsg::kDevInfo)
	{
		LogWarning("Connection to %s dropped (expected devInfo, got %d instead)\n",
			hostname.c_str(), dinfo.Payload_case());
		return;
	}
	auto dinfom = dinfo.devinfo();
	LogVerbose("(architecture is %s)\n", dinfom.arch().c_str());

	while(true)
	{
		//Expect fileChanged or fileRemoved messages
		SplashMsg msg;
		if(!RecvMessage(s, msg, hostname))
			break;

		switch(msg.Payload_case())
		{
			case SplashMsg::kBulkFileChanged:
				if(!OnBulkFileChanged(s, msg.bulkfilechanged(), hostname, id))
					return;
				break;

			case SplashMsg::kBulkFileData:
				if(!OnBulkFileData(s, msg.bulkfiledata(), hostname, id))
					return;
				break;

			case SplashMsg::kFileRemoved:
				if(!OnFileRemoved(msg.fileremoved(), hostname, id))
					return;
				break;

			default:
				LogWarning("Connection to %s dropped (bad message type in event header)\n", hostname.c_str());
				return;
		}
	}
}

/**
	@brief Process a msgFileRemoved
 */
bool OnFileRemoved(const FileRemoved& msg, string& hostname, clientID id)
{
	LogVerbose("File %s on node %s deleted\n",
		msg.fname().c_str(),
		hostname.c_str());

	//Update the file's status in our working copy
	g_nodeManager->GetWorkingCopy(id)->RemoveFile(msg.fname());
	return true;
}

/**
	@brief Process a msgBulkFileData
 */
bool OnBulkFileData(Socket& /*s*/, const BulkFileData& msg, string& /*hostname*/, clientID id)
{
	auto wc = g_nodeManager->GetWorkingCopy(id);

	//Add all of the files to the cache.
	//Update the working copy with everything but build scripts
	set<string> dirtyScripts;
	for(int i=0; i<msg.data_size(); i++)
	{
		auto d = msg.data(i);
		string fname = d.fname();
		//LogDebug("Source file %s changed\n", fname.c_str());
		g_cache->AddFile(GetBasenameOfFile(fname), d.id(), d.hash(), d.filedata(), "");
		if(GetBasenameOfFile(fname) != "build.yml")
			wc->UpdateFile(fname, d.hash(), true, false, dirtyScripts);
	}

	//Process build scripts once the files they depend on are done
	for(int i=0; i<msg.data_size(); i++)
	{
		auto d = msg.data(i);
		string fname = d.fname();
		if(GetBasenameOfFile(fname) == "build.yml")
			wc->UpdateFile(fname, d.hash(), true, true, dirtyScripts);
	}

	//If this change caused a script to become dirty, re-run that script.
	//TODO: Recursively update scripts (but don't update ones we've updated during this round)
	set<string> ignored;
	for(auto f : dirtyScripts)
	{
		LogVerbose("Re-running script %s because it contains targets depending on a target we updated\n", f.c_str());
		wc->UpdateFile(f, wc->GetFileHash(f), true, true, ignored);
	}

	return true;
}

/**
	@brief Process a msgBulkFileChanged
 */
bool OnBulkFileChanged(Socket& s, const BulkFileChanged& msg, string& hostname, clientID id)
{
	LogDebug("BulkFileChanged\n");
	LogIndenter li;

	//Build the response
	SplashMsg ack;
	auto ackm = ack.mutable_bulkfileack();

	//Ask for anything we're missing
	bool missingFiles = false;
	for(int i=0; i<msg.files_size(); i++)
	{
		auto mfc = msg.files(i);

		string fname = mfc.fname();
		string hash = mfc.hash();

		//See if we have the file in the global cache
		//This is a source file since it's in a client's working copy.
		//As a result, the object ID is just the sha256sum of the file itself
		bool hit = g_cache->IsCached(hash);

		//Report status to the client
		auto fack = ackm->add_acks();
		fack->set_fname(fname);
		fack->set_filecached(hit);

		if(!hit)
			missingFiles = true;
	}

	//Send the acknowledgement to the client
	if(!SendMessage(s, ack, hostname))
		return false;

	//If we were missing any files, wait for their contents
	if(missingFiles)
	{
		SplashMsg msg;
		if(!RecvMessage(s, msg, hostname))
			return false;
		if(msg.Payload_case() != SplashMsg::kBulkFileData)
		{
			LogWarning("Connection to %s dropped (bad message type in event header)\n", hostname.c_str());
			return false;
		}
		auto data = msg.bulkfiledata();

		//Do limited processing (just push content into cache)
		for(int i=0; i<data.data_size(); i++)
		{
			auto d = data.data(i);
			g_cache->AddFile(GetBasenameOfFile(d.fname()), d.id(), d.hash(), d.filedata(), "");
		}
	}

	//Finally, process the files in the order the client asked us to.
	//Do two passes, source first then scripts
	set<string> dirtyScripts;
	for(int i=0; i<msg.files_size(); i++)
	{
		auto mfc = msg.files(i);
		string fname = mfc.fname();
		if(GetBasenameOfFile(fname) != "build.yml")
			g_nodeManager->GetWorkingCopy(id)->UpdateFile(mfc.fname(), mfc.hash(), mfc.body(), mfc.config(), dirtyScripts);
	}

	//Process build scripts once the files they depend on are done
	for(int i=0; i<msg.files_size(); i++)
	{
		auto mfc = msg.files(i);
		string fname = mfc.fname();
		if(GetBasenameOfFile(fname) == "build.yml")
			g_nodeManager->GetWorkingCopy(id)->UpdateFile(mfc.fname(), mfc.hash(), mfc.body(), mfc.config(), dirtyScripts);
	}

	//If this change caused a script to become dirty, re-run that script.
	//TODO: Recursively update scripts (but don't update ones we've updated during this round)
	auto wc = g_nodeManager->GetWorkingCopy(id);
	set<string> ignored;
	for(auto f : dirtyScripts)
	{
		LogVerbose("Re-running script %s because it contains targets depending on a file we updated\n", f.c_str());
		wc->UpdateFile(f, wc->GetFileHash(f), true, true, ignored);
	}

	//and rebuild the dependency graph
	wc->GetGraph().Rebuild();

	return true;
}
