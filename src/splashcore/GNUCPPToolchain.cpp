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

GNUCPPToolchain::GNUCPPToolchain(string basepath, string triplet)
	: CPPToolchain(basepath, TOOLCHAIN_GNU)
	, GNUToolchain(triplet, basepath, GNU_CPP)
{
	//Get the full compiler version
	m_stringVersion = string("GNU C++") + ShellCommand(basepath + " --version | head -n 1 | cut -d \")\" -f 2");

	//Parse it
	if(3 != sscanf(m_stringVersion.c_str(), "GNU C++ %4d.%4d.%4d",
		&m_majorVersion, &m_minorVersion, &m_patchVersion))
	{
		//TODO: handle this better, don't abort :P
		LogFatal("bad G++ version\n");
	}

	//Some compilers can target other arches if you feed them the right flags.
	//Thanks to @GyrosGeier for this
	string cmd =
		string("/bin/bash -c \"") +
		basepath + " -print-multi-lib | sed -e 's/.*;//' -e 's/@/ -/g' | while read line; do " +
		basepath + " \\$line -print-multiarch; done" +
		string("\"");
	string extra_arches = ShellCommand(cmd);
	vector<string> triplets;
	ParseLines(extra_arches, triplets);
	for(auto t : triplets)
		m_triplets.emplace(t);

	//If no arches found in the last step, fall back to the triplet in the file name
	if(m_triplets.empty())
		m_triplets.emplace(triplet);

	//Look up where this toolchain gets its include files from
	for(auto t : m_triplets)
	{
		vector<string> paths;
		FindDefaultIncludePaths(paths, basepath, true, t);
		m_defaultIncludePaths[t] = paths;
		m_virtualSystemIncludePath[t] =
			"__sysinclude__/" + str_replace(" ", "_", m_stringVersion) + "_" + t;
	}

	//Set suffixes for WINDOWS
	if(triplet.find("mingw") != string::npos)
	{
		m_exeSuffix = ".exe";
		m_shlibSuffix = ".dll";
		m_stlibSuffix = ".lib";
		m_objSuffix = ".obj";
		m_shlibPrefix = "";
	}

	//Set suffixes for POSIX
	else
	{
		m_exeSuffix = "";
		m_shlibSuffix = ".so";
		m_stlibSuffix = ".a";
		m_objSuffix = ".o";
		m_shlibPrefix = "lib";
	}

	//Generate the hash
	//TODO: Anything else to add here?
	m_hash = sha256(m_stringVersion + triplet);
}

GNUCPPToolchain::~GNUCPPToolchain()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Toolchain properties

set<string> GNUCPPToolchain::GetToolchainDependencies(string arch)
{
	return GNUToolchain::GetLibrariesForArch(arch);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual operations

bool GNUCPPToolchain::ScanDependencies(
	string arch,
	string path,
	string root,
	set<BuildFlag> flags,
	set<string>& deps,
	map<string, string>& dephashes,
	string& output,
	set<string>& missingFiles)
{
	return GNUToolchain::ScanDependencies(
		m_basepath,
		arch,
		path,
		root,
		flags,
		m_defaultIncludePaths[arch],
		deps,
		dephashes,
		output,
		missingFiles,
		true);
}

/**
	@brief Compile stuff
 */
bool GNUCPPToolchain::Build(
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& output)
{
	return GNUToolchain::Compile(m_basepath, triplet, sources, fname, flags, outputs, output, true);
}
