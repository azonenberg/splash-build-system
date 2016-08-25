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

void BuildClientThread(Socket& s, string& hostname)
{
	LogNotice("Build server %s connected\n", hostname.c_str());
	
	//Expect a BuildInfo message
	msgBuildInfo binfo;
	if(!s.RecvLooped((unsigned char*)&binfo, sizeof(binfo)))
	{
		LogWarning("Connection from %s dropped (while getting buildInfo)\n", hostname.c_str());
		return;
	}
	if(binfo.type != MSG_TYPE_BUILD_INFO)
	{
		LogWarning("Connection from %s dropped (bad message type in buildInfo)\n", hostname.c_str());
		return;
	}
	
	//Print stats
	LogVerbose("Build server %s has %d CPU cores, speed %d, RAM capacity %d MB, %d toolchains installed\n",
		hostname.c_str(), binfo.cpuCount, binfo.cpuSpeed, binfo.ramSize, binfo.toolchainCount);
		
	//Read the toolchains
	for(unsigned int i=0; i<binfo.toolchainCount; i++)
	{
		//Get the header
		msgAddCompiler tadd;
		if(!s.RecvLooped((unsigned char*)&tadd, sizeof(tadd)))
		{
			LogWarning("Connection from %s dropped (while getting addCompiler header)\n", hostname.c_str());
			return;
		}
		
		//Read the hash
		string hash;
		if(!s.RecvPascalString(hash))
		{
			LogWarning("Connection from %s dropped (while getting addCompiler hash)\n", hostname.c_str());
			return;
		}
		
		//and compiler version
		string ver;
		if(!s.RecvPascalString(ver))
		{
			LogWarning("Connection from %s dropped (while getting addCompiler ver)\n", hostname.c_str());
			return;
		}
		
		//Languages
		for(unsigned int j=0; j<tadd.numLangs; j++)
		{			
			uint8_t lang;
			if(!s.RecvLooped((unsigned char*)&lang, 1))
			{
				LogWarning("Connection from %s dropped (while getting addCompiler lang)\n", hostname.c_str());
				return;
			}
		}
		
		//Triplets
		for(unsigned int j=0; j<tadd.numTriplets; j++)
		{
			string triplet;
			if(!s.RecvPascalString(triplet))
			{
				LogWarning("Connection from %s dropped (while getting addCompiler triplet %u/%u)\n",
					hostname.c_str(), j, tadd.numTriplets);
				return;
			}
		}
	}
}
