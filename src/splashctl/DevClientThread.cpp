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

void DevClientThread(Socket& s, string& hostname, clientID id)
{
	LogNotice("Developer workstation %s connected\n", hostname.c_str());
	
	//Expect a DevInfo message
	msgDevInfo dinfo;
	if(!s.RecvLooped((unsigned char*)&dinfo, sizeof(dinfo)))
	{
		LogWarning("Connection from %s dropped (while getting devInfo)\n", hostname.c_str());
		return;
	}
	if(dinfo.type != MSG_TYPE_DEV_INFO)
	{
		LogWarning("Connection from %s dropped (bad message type in devInfo)\n", hostname.c_str());
		return;
	}
	string arch;
	if(!s.RecvPascalString(arch))
	{
		LogWarning("Connection from %s dropped (while getting devInfo arch)\n", hostname.c_str());
		return;
	}
	LogVerbose("    (architecture is %s)\n", arch.c_str());
	
	while(true)
	{
		//Expect fileChanged messages (all build requests etc will come through from a "splash" client)
		msgFileChanged mfc;
		if(!s.RecvLooped((unsigned char*)&mfc, sizeof(mfc)))
			break;
		if(mfc.type != MSG_TYPE_FILE_CHANGED)
		{
			LogWarning("Connection from %s dropped (bad message type in fileChanged)\n", hostname.c_str());
			return;
		}
		string fname;
		if(!s.RecvPascalString(fname))
		{
			LogWarning("Connection from %s dropped (while getting fileChanged.fname)\n", hostname.c_str());
			return;
		}
		string hash;
		if(!s.RecvPascalString(hash))
		{
			LogWarning("Connection from %s dropped (while getting fileChanged.hash)\n", hostname.c_str());
			return;
		}
				
		//See if we have the file in the global cache
		//This is a source file since it's in a client's working copy.
		//As a result, the object ID is just the sha256sum of the file itself
		bool hit = g_cache->IsCached(hash);
		
		//Debug print
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
		
		//Report status to the client
		msgFileAck ack;
		ack.fileCached = hit;
		if(!s.SendLooped((unsigned char*)&ack, sizeof(ack)))
		{
			LogWarning("Connection from %s dropped (while sending fileAck)\n", hostname.c_str());
			return;
		}
		
		//If it's cached we don't have to do anything more now
		if(hit)
			continue;

		//Get the file contents
		msgFileData data;
		if(!s.RecvLooped((unsigned char*)&data, sizeof(data)))
		{
			LogWarning("Connection from %s dropped while reading fileData\n", hostname.c_str());
			return;
		}
		if(data.type != MSG_TYPE_FILE_DATA)
		{
			LogWarning("Connection from %s dropped (bad message type in fileData)\n", hostname.c_str());
			return;
		}
		if(data.fileLen > INT_MAX)
		{
			LogWarning("Connection from %s dropped (bad file length in fileData)\n", hostname.c_str());
			return;
		}
		unsigned char* buf = new unsigned char[data.fileLen];
		if(!s.RecvLooped(buf, data.fileLen))
		{
			LogWarning("Connection from %s dropped while reading fileData.data\n", hostname.c_str());
			return;
		}
				
		//Write the file to cache
		g_cache->AddFile(GetBasenameOfFile(fname), hash, hash, buf, data.fileLen);
		
		//Update the file's status in our working copy
		
		//Clean up
		delete[] buf;
	}
}
