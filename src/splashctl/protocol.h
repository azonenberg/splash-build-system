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
	Conventions
		All multi-byte fields are sent LITTLE ENDIAN (for efficiency with x86 endpoints).
		All text strings use Pascal style formatting: 16 bit length, then ASCII, no null terminator
		All arrays have a 32-bit count, then elements
	
	Packet format
		2 byte msgType is header of every message
		Remainder is message dependent
	
	ALL CLIENTS
		Connection setup
			client:	MSG_TYPE_CLIENTHELLO
			server:	MSG_TYPE_SERVERHELLO
			If magic number or version numbers are bad, drop the connection immediately
			Else, use clientType to activate the proper server logic for that type of client
		
	BUILD CLIENT
		Connection setup
			client: MSG_TYPE_BUILD_INFO
			client:	MSG_TYPE_ADD_COMPILER (repeated once for each compiler on the node)
	
	DEVELOPER CLIENT
		Connection setup
			client: MSG_TYPE_DEV_INFO
			
		Initial scan of working copy
			client: MSG_TYPE_FILE_CHANGED (for every file in the working copy)
			
		File changed
			client: MSG_TYPE_FILE_CHANGED
			server: MSG_TYPE_FILE_ACK
		
		If file isn't in cache:
			client: MSG_TYPE_FILE_DATA

		File removed
			client: MSG_TYPE_FILE_REMOVED
			
		TODO: File moved/deleted
 */

//Message type (first 2 bytes of every message)
enum msgType
{
	MSG_TYPE_CLIENTHELLO,
	MSG_TYPE_SERVERHELLO,
	MSG_TYPE_BUILD_INFO,
	MSG_TYPE_ADD_COMPILER,
	MSG_TYPE_DEV_INFO,
	MSG_TYPE_FILE_CHANGED,
	MSG_TYPE_FILE_ACK,
	MSG_TYPE_FILE_DATA,
	MSG_TYPE_FILE_REMOVED,
	
	//all types beyond here reserved for expansion
	MSG_TYPE_LAST
};

//Type field for MSG_TYPE_CLIENTHELLO
enum clientType
{
	CLIENT_DEVELOPER,
	CLIENT_BUILD,
	
	//all types beyond here reserved for expansion
	CLIENT_LAST
};

class msg
{
public:
	msg(uint16_t t)
	: type(t)
	{}

	uint16_t		type;
} __attribute__ ((packed));

//Identify the client
class msgClientHello : public msg
{
public:
	msgClientHello(uint8_t ctp)
		: msg(MSG_TYPE_CLIENTHELLO)
		, magic(0x444c4942)	//"BILD"
		, clientVersion(1)
		, ctype(ctp)
	{}
	
	uint32_t magic;			//magic number
	uint16_t clientVersion;	//protocol version supported by server (always 1 for now)
	uint8_t ctype;			//clientType
	
	//After this struct:
	//hostname		string
	
} __attribute__ ((packed)); 

//Identify the server to clients
class msgServerHello : public msg
{
public:
	msgServerHello()
		: msg(MSG_TYPE_SERVERHELLO)
		, magic(0x444c4942)	//"BILD"
		, serverVersion(1)
	{}

	uint32_t magic;			//magic number
	uint16_t serverVersion;	//protocol version supported by server (always 1 for now)
	
} __attribute__ ((packed));

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

//Report basic information about a developer client
class msgDevInfo : public msg
{
public:
	msgDevInfo()
		: msg(MSG_TYPE_DEV_INFO)
	{}
	
	//After this struct:
	//arch			string		Client OS architecture triplet
};

//Report that a file changed clientside
class msgFileChanged : public msg
{
public:
	msgFileChanged()
		: msg(MSG_TYPE_FILE_CHANGED)
	{}
	
	//After this struct:
	//fname				string		Path of changed file
	//hash				string		ASCII hex SHA-256 sum of the changed file
};

//TODO: Decide how to do the rest of this... old notes
//TODO: deletion of build scripts? Are those parsed client or server side?
//Directory creation/destruction is irrelevant to the build
//TODO: file moves

//Tell the client whether the most recently changed file is in cache or not
class msgFileAck : public msg
{
public:
	msgFileAck()
		: msg(MSG_TYPE_FILE_ACK)
	{}
	
	uint8_t		fileCached;			//1 if file is in cache, 0 if we have to send it
};

//Client sends file content to server
class msgFileData : public msg
{
public:
	msgFileData()
		: msg(MSG_TYPE_FILE_DATA)
	{}
	
	uint64_t	fileLen;			//length of file
	
	//After this struct:
	//data[]						Content of file
};

//Report that a file was deleted clientside
class msgFileRemoved : public msg
{
public:
	msgFileRemoved()
		: msg(MSG_TYPE_FILE_REMOVED)
	{}
	
	//After this struct:
	//fname				string		Path of changed file
};

#endif

