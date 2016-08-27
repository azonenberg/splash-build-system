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

void DevClientThread(Socket& s, string& hostname)
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
		LogWarning("Connection from %s dropped (while getting devInfo arch)\n", arch.c_str());
		return;
	}
	LogVerbose("    (architecture is %s)\n", arch.c_str());
	
	/*
	//Print stats
	LogVerbose("Build server %s has %d CPU cores, speed %d, RAM capacity %d MB, %d toolchains installed\n",
		hostname.c_str(), binfo.cpuCount, binfo.cpuSpeed, binfo.ramSize, binfo.toolchainCount);
		
	//If we already have this node registered, complain and drop it
	//TODO: drop the old node instead?
	if(g_activeClients.find(hostname) != g_activeClients.end())
	{
		LogWarning("Connection from %s dropped (already connected)\n", hostname.c_str());
		return;
	}
		
	//If no toolchains, just quit now
	if(binfo.toolchainCount == 0)
	{
		LogWarning("Connection from %s dropped (no toolchains found)\n", hostname.c_str());
		return;
	}
	*/
}
