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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ToolchainSettings::ToolchainSettings()
{

}

ToolchainSettings::~ToolchainSettings()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Build script parsing

/**
	@brief Removes configurations declared in the supplied path
 */
void ToolchainSettings::PurgeConfig(string path)
{
	m_recursiveSettings.erase(path);
	m_fileSettings.erase(path);
}

/**
	@brief Loads configuration from a YAML node
 */
void ToolchainSettings::LoadConfig(YAML::Node& node, bool recursive, string path)
{
	if(recursive)
		m_recursiveSettings[path] = BuildSettings(node);
	else
		m_fileSettings[path] = BuildSettings(node);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Data accessors

void ToolchainSettings::GetDefaultArchitectures(unordered_set<string>& arches, string path)
{
	//Split the path up into segments
	list<string> dirs;
	string p = path;
	while(p != "")
	{
		p = GetDirOfFile(p);
		dirs.push_front(p);
	}
	
	//Start out with an empty list of architectures
	arches.clear();
	
	//For each directory from top down to us, look up the settings for that directory
	for(auto d : dirs)
	{
		//Generate the path to the build script
		string p = d;
		if(p != "")
			p += "/";
		p += "build.yml";
		
		//See if there are any recursive settings for us there.
		//If not, automatically inherit from parent scope
		if(m_recursiveSettings.find(p) == m_recursiveSettings.end())
			continue;
			
		GetDefaultArchitectures_helper(m_recursiveSettings[p], arches);
	}
	
	//Search our path for file level settings
	if(m_fileSettings.find(path) == m_fileSettings.end())
		return;
	GetDefaultArchitectures_helper(m_fileSettings[path], arches);
}

void ToolchainSettings::GetDefaultArchitectures_helper(const BuildSettings& settings, unordered_set<string>& arches)
{
	//If the flags do not include "global", clear whatever was there before
	if(!settings.InheritTriplets())
		arches.clear();
	
	//Copy the current flags
	auto triplets = settings.GetTriplets();
	for(auto t : triplets)
		arches.emplace(t);
}
