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

#include "splashcore.h"

using namespace std;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

GNULinkerToolchain::GNULinkerToolchain(string basepath, string triplet)
	: LinkerToolchain(basepath, TOOLCHAIN_GNU)
{
	//Get the full toolchain version
	m_stringVersion = string("GNU Linker") + ShellCommand(basepath + " --version | head -n 1 | cut -d \")\" -f 2");
	
	//Parse it
	if(2 != sscanf(m_stringVersion.c_str(), "GNU Linker %4d.%4d",
		&m_majorVersion, &m_minorVersion))
	{
		//TODO: handle this better, don't abort :P
		LogFatal("bad ld version\n");
	}
	m_patchVersion = 0;

	//Get the list of supported $ARCHes
	/*
	string archlist = ShellCommand(basepath + " -V | grep elf");
	vector<string> lines;
	ParseLines(lines, archlist);
	*/
	
	//Use the arch in the file name
	m_triplets.push_back(triplet);

	//Add some extra triplets as needed
	//TODO: can we auto-detect this without having to hard code mappings?
	if(triplet == "x86_64-linux-gnu")
	{
		m_triplets.push_back(str_replace("x86_64", "i386", triplet));
		m_triplets.push_back(str_replace("gnu", "gnux32", triplet));
	}
	//TODO: mips/mipsel etc
	
	//Generate the hash
	//TODO: Anything else to add here?
	m_hash = sha256(m_stringVersion + triplet);
}

GNULinkerToolchain::~GNULinkerToolchain()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Toolchain properties

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual operations

bool GNULinkerToolchain::ScanDependencies(
	string /*path*/,
	string /*root*/,
	set<BuildFlag> /*flags*/,
	set<string>& /*deps*/)
{
	LogError("GNULinkerToolchain::ScanDependencies() is meaningless for now\n");
	return false;
}
