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
	LogIndenter li;

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
bool OnBulkFileData(Socket& s, const BulkFileData& msg, string& hostname, clientID id)
{
	LogDebug("Got a bulk file data\n");

	for(int i=0; i<msg.data_size(); i++)
	{
		auto d = msg.data(i);

		string fname = d.fname();

		//Write the file to cache
		g_cache->AddFile(GetBasenameOfFile(fname), d.id(), d.hash(), d.filedata(), "");

		//Update the file's status in our working copy
		//(and re-run build scripts if needed)
		g_nodeManager->GetWorkingCopy(id)->UpdateFile(fname, d.hash(), true, true);
	}

	return true;
}

/**
	@brief Process a msgBulkFileChanged
 */
bool OnBulkFileChanged(Socket& s, const BulkFileChanged& msg, string& hostname, clientID id)
{
	//Build the response
	SplashMsg ack;
	auto ackm = ack.mutable_bulkfileack();

	for(int i=0; i<msg.files_size(); i++)
	{
		auto mfc = msg.files(i);

		string fname = mfc.fname();
		string hash = mfc.hash();

		//See if we have the file in the global cache
		//This is a source file since it's in a client's working copy.
		//As a result, the object ID is just the sha256sum of the file itself
		bool hit = g_cache->IsCached(hash);

		//Debug print
		/*
		if(hit)
		{
			LogVerbose("File %s on node %s changed (new version already in cache)\n",
				fname.c_str(),
				hostname.c_str());
		}
		else
		{
			LogVerbose("File %s on node %s changed (fetching content from client)\n",
				fname.c_str(),
				hostname.c_str());
		}
		*/

		//Report status to the client
		auto fack = ackm->add_acks();
		fack->set_fname(fname);
		fack->set_filecached(hit);

		//If it's in the cache, update the status now
		//(if not, wait until the file data comes in)
		if(hit)
			g_nodeManager->GetWorkingCopy(id)->UpdateFile(fname, hash, mfc.body(), mfc.config());
	}

	//Send it to the client
	if(!SendMessage(s, ack, hostname))
		return false;

	return true;
}
