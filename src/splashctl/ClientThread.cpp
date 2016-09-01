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
#include <ext/stdio_filebuf.h>

using namespace std;

void ClientThread(ZSOCKET sock)
{
	Socket s(sock);
	
	string client_hostname = "[no hostname]";
	//LogDebug("New connection received from %s\n", client_hostname.c_str());
	
	//Send a server hello
	SplashMsg shi;
	auto shim = shi.mutable_serverhello();
	shim->set_magic(SPLASH_PROTO_MAGIC);
	shim->set_version(SPLASH_PROTO_VERSION);
	if(!SendMessage(s, shi, client_hostname))
		return;
	
	//Get a client hello
	SplashMsg chi;
	if(!RecvMessage(s, chi, client_hostname))
		return;
	if(chi.Payload_case() != SplashMsg::kClientHello)
	{
		LogWarning("Connection from %s dropped (expected clientHello, got %d instead)\n",
			client_hostname.c_str(), chi.Payload_case());
		return;
	}
	auto chim = chi.clienthello();
	if(chim.magic() != SPLASH_PROTO_MAGIC)
	{
		LogWarning("Connection from %s dropped (bad magic number in clientHello)\n", client_hostname.c_str());
		return;
	}
	if(chim.version() != SPLASH_PROTO_VERSION)
	{
		LogWarning("Connection from %s dropped (bad version number in clientHello)\n", client_hostname.c_str());
		return;
	}
	client_hostname = chim.hostname();
	
	//If hostname is alphanumeric or - chars, fail
	for(size_t i=0; i<client_hostname.length(); i++)
	{
		auto c = client_hostname[i];
		if(isalnum(c) || (c == '-') )
			continue;
		
		LogWarning("Connection from %s dropped (bad character in hostname)\n", client_hostname.c_str());
		return;
	}
	
	//Assign a unique ID to the client
	clientID id = g_nodeManager->AllocateClient(client_hostname);
	
	//Protocol-specific processing
	switch(chim.type())
	{
		case ClientHello::CLIENT_DEVELOPER:
			
			//Process client traffic
			DevClientThread(s, client_hostname, id);
			
			//Clean up
			g_nodeManager->RemoveClient(id);
			LogVerbose("Developer workstation %s disconnected\n", client_hostname.c_str());
			break;
			
		case ClientHello::CLIENT_BUILD:
			
			//Process client traffic
			BuildClientThread(s, client_hostname, id);
			
			//Clean up
			g_nodeManager->RemoveClient(id);
			LogVerbose("Build server %s disconnected\n", client_hostname.c_str());
			break;
		
		default:
			LogWarning("Connection from %s dropped (bad client type)\n", client_hostname.c_str());
			break;
	}
}
