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

void ClientThread(ZSOCKET sock)
{
	Socket s(sock);
	
	string client_hostname = "[no hostname]";
	
	LogNotice("New connection received from %s\n", client_hostname.c_str());
	
	//Send it a server hello
	msgServerHello shi;
	if(!s.SendLooped((unsigned char*)&shi, sizeof(shi)))
	{
		LogNotice("Connection from %s dropped (while sending serverHello)\n", client_hostname.c_str());
		return;
	}
	
	//Get a client hello
	msgClientHello chi(CLIENT_LAST);
	if(!s.RecvLooped((unsigned char*)&chi, sizeof(chi)))
	{
		LogNotice("Connection from %s dropped (while getting clientHello)\n", client_hostname.c_str());
		return;
	}
	if(chi.magic != shi.magic)
	{
		LogNotice("Connection from %s dropped (bad magic number in clientHello)\n", client_hostname.c_str());
		return;
	}
	if(chi.clientVersion != 1)
	{
		LogNotice("Connection from %s dropped (bad version number in clientHello)\n", client_hostname.c_str());
		return;
	}
	if(!s.RecvPascalString(client_hostname))
		return;
	
	//If hostname is alphanumeric or - chars, fail
	for(size_t i=0; i<client_hostname.length(); i++)
	{
		auto c = client_hostname[i];
		if(isalnum(c) || (c == '-') )
			continue;
		
		LogNotice("Connection from %s dropped (bad character in hostname)\n", client_hostname.c_str());
		return;
	}
	
	//Protocol-specific processing
	switch(chi.type)
	{
		case CLIENT_DEVELOPER:
			LogError("Developer clients not implemented\n");
			break;
			
		case CLIENT_BUILD:
			BuildClientThread(s, client_hostname);
			break;
		
		default:
			LogNotice("Connection from %s dropped (bad client type)\n", client_hostname.c_str());
			break;
	}
}
