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
			
		TODO: File moved/deleted
			
		File needed server side
			server: MSG_TYPE_FILE_REQUEST
			client: MSG_TYPE_FILE_DATA
 */

//Message type (first 2 bytes of every message)
enum msgType
{
	//Identify the client
	//magic 			uint32		"BILD"
	//clientVersion		uint16		protocol version supported by client (always 1 for now)
	//clientType		uint8		type of client, all further processing depends on this
	//hostname 			string		host name of client (for logging etc)
	MSG_TYPE_CLIENTHELLO,
	
	//Identify the server
	//magic				uint32		"BILD"
	//serverVersion		uint16		protocol version supported by server (always 1 for now)
	MSG_TYPE_SERVERHELLO,
	
	//Report basic info about a build server, so we can schedule jobs more effectively
	//cpuCount			uint16		Number of logical CPU cores on the system
	//cpuSpeed			uint32		Processor speed according to some TBD benchmark.
	//								Used to prioritize jobs so faster servers get work first
	//								when the cluster is lightly loaded
	//ramSize			uint16		Amount of memory on the system, in GB
	//TODO: ram speed?
	MSG_TYPE_BUILD_INFO,
	
	//Add a new compiler to the list of compilers present on a given build server
	//compilerType 		uint8		Type of compiler (CToolchain::CompilerType)
	//versionStr 		string		Human-readable / hashable version string of compiler
	//versionNum 		uint32		Machine-readable version number of compiler, left justified hex
	//								4.9.2 is encoded as 0x04090200
	//langs 			uint8 arr	Array of languages we support
	//triplets			str arr		Array of triplets we can target
	MSG_TYPE_ADD_COMPILER,
	
	//Report basic information about a developer client
	//arch				string		Client OS architecture triplet
	MSG_TYPE_DEV_INFO,
	
	//Report that a file changed clientside
	//Note that we do not report file deletion; deleted files stay in cache until evicted.
	//TODO: immediate report for deletion of build scripts? Are those parsed client or server side?
	//Directory creation/destruction is also irrelevant to the build, as is file moving.
	//fname				string		Path of changed file
	//hash				string		ASCII hex SHA-256 sum of the changed file
	MSG_TYPE_FILE_CHANGED,
	
	//Server requests the contents of a file from the client
	//fname				string		Path of changed file
	MSG_TYPE_FILE_REQUEST,
	
	//Client sends the contents of a file to the server
	//fname				string		Path of changed file
	//data				uint8 arr	Contents of file
	MSG_TYPE_FILE_DATA,
	
	//all types beyond here reserved for expansion
	MSG_TYPE_LAST
};

//Type field for MSG_TYPE_CLIENTHELLO
enum clientType
{
	CLIENT_DEVELOPER,
	CLIENT_BUILD
};

#endif

