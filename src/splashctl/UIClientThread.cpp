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

void UIClientThread(Socket& s, string& hostname, clientID id)
{
	LogNotice("Developer client %s connected\n", hostname.c_str());

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
	LogVerbose("    (architecture is %s)\n", dinfom.arch().c_str());

	while(true)
	{
		//Expect fileChanged or fileRemoved messages
		SplashMsg msg;
		if(!RecvMessage(s, msg, hostname))
			break;

		switch(msg.Payload_case())
		{
			/*
			case SplashMsg::kFileChanged:
				if(!OnFileChanged(s, msg.filechanged(), hostname, id))
					return;
				break;

			case SplashMsg::kFileRemoved:
				if(!OnFileRemoved(msg.fileremoved(), hostname, id))
					return;
				break;
			*/
			default:
				LogWarning("Connection to %s [%d] dropped (bad message type in event header)\n",
					hostname.c_str(), id);
				return;
		}
	}
}
