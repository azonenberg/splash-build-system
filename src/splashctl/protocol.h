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

#ifndef protocol_h
#define protocol_h
/*
//Report basic info about a build server, so we can schedule jobs more effectively
class msgBuildInfo : public msg
{
public:
	msgBuildInfo()
		: msg(MSG_TYPE_BUILD_INFO)
	{}

	uint16_t cpuCount;		//Number of logical CPU cores on the system
	uint32_t cpuSpeed;		//Processor speed according to some arbitrary benchmark.
							//Used to prioritize jobs so faster servers get work first
							//when the cluster is lightly loaded.
							//For now just use bogomips, TODO something more accurate
	uint16_t ramSize;		//RAM capacity, in MB
	//TODO: ram speed?
	
	uint16_t toolchainCount;//Number of toolchains on the node
	
} __attribute__ ((packed));

//Report compilers installed on the build server
class msgAddCompiler : public msg
{
public:
	msgAddCompiler()
		: msg(MSG_TYPE_ADD_COMPILER)
	{}
	
	uint8_t compilerType;			//Type of compiler (Toolchain::CompilerType)
	uint32_t versionNum;			//Machine-readable version number of compiler, left justified hex
									//4.9.2 is encoded as 0x04090200
									//This coding is easier to quickly compare greater/less than loose ints
	uint16_t numLangs;				//number of languages we support
	uint16_t numTriplets;			//number of architecture triplets we support
	
	//After this struct:
	//string		hash
	//string		versionStr
	//uint8[]		langs
	//string[]		triplets
} __attribute__ ((packed));

*/
#endif

