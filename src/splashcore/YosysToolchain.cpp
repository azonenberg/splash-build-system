/***********************************************************************************************************************
*                                                                                                                      *
* SPLASH build system v0.2                                                                                             *
*                                                                                                                      *
* Copyright (c) 2016-2017 Andrew D. Zonenberg                                                                          *
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

YosysToolchain::YosysToolchain(string basepath)
	: FPGAToolchain(basepath, TOOLCHAIN_YOSYS)
{
	//Save version info
	string sver = ShellCommand(basepath + " -V");
	sscanf(sver.c_str(), "Yosys %8d.%8d+%8d", &m_majorVersion, &m_minorVersion, &m_patchVersion);

	char tmp[128];
	snprintf(tmp, sizeof(tmp), "Yosys %d.%d+%d", m_majorVersion, m_minorVersion, m_patchVersion);
	m_stringVersion = tmp;

	//Set list of target architectures
	FindArchitectures();

	//File format suffixes
	m_fixes["formal"] = stringpair("", ".txt");

	//Generate the hash based on the full git ID etc
	m_hash = sha256(sver);
}

YosysToolchain::~YosysToolchain()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Search for other toolchains

void YosysToolchain::FindArchitectures()
{
	//We always support formal targets b/c those don't need any extra tools
	m_triplets.emplace("generic-formal");

	//Get our search path
	vector<string> dirs;
	ParseSearchPath(dirs);

	//TODO: IceStorm stuff

	//Look for gp4par
	m_gp4parPath = FindExecutable("gp4par", dirs);
	if(m_gp4parPath != "")
	{
		m_triplets.emplace("greenpak4-slg46140");
		m_triplets.emplace("greenpak4-slg46620");
		m_triplets.emplace("greenpak4-slg46621");
	}
}

/**
	@brief Look for a binary anywhere in the search path
 */
string YosysToolchain::FindExecutable(string fname, vector<string>& dirs)
{
	for(auto dir : dirs)
	{
		string path = dir + "/" + fname;
		if(DoesFileExist(path))
			return path;
	}

	return "";
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Toolchain properties

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual compilation

bool YosysToolchain::Build(
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	//Switch on the triplet to decide how to handle it
	if(triplet == "generic-formal")
		return BuildFormal(triplet, sources, fname, flags, outputs, stdout);
	else if(triplet.find("greenpak4-") == 0)
		return BuildGreenPAK(triplet, sources, fname, flags, outputs, stdout);

	else
	{
		stdout = string("ERROR: YosysToolchain doesn't know how to build for architecture ") + triplet + "\n";
		return false;
	}
}

/**
	@brief Synthesize and run the formal model checker
 */
bool YosysToolchain::BuildFormal(
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	stdout = "ERROR: YosysToolchain::BuildFormal() not implemented\n";
	return false;
}

bool YosysToolchain::BuildGreenPAK(
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	stdout = "ERROR: YosysToolchain::BuildGreenPAK() not implemented\n";
	return false;
}
