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

BuildSettings::BuildSettings()
{
	//default ctor for STL, not normally used directly
}

BuildSettings::BuildSettings(YAML::Node& node)
	: m_bInheritTriplets(false)
	, m_bInheritFlags(false)
{
	//we already read the name and toolchain by the time we got here, ignore them
	
	//Target architectures
	if(node["arches"])
	{
		auto arches = node["arches"];
		for(auto it : arches)
		{
			string t = it.as<std::string>();
			
			//special keywords for triplet inheritance
			if(t == "global")
				m_bInheritTriplets = true;
			
			//nope, just copy it
			else
				m_triplets.emplace(t);
		}
	}
	
	//Compiler flags
	if(node["flags"])
	{
		auto flags = node["flags"];
		for(auto it : flags)
		{
			string flag = it.as<std::string>();
			
			//special keywords for flag inheritance
			if(flag == "global")
				m_bInheritFlags = true;
				
			//nope, just create a flag
			else
				m_flags.emplace(BuildFlag(flag));
		}
	}
	
	//Build configurations
	if(node["configurations"])
	{
		auto configs = node["configurations"];
		for(auto it : configs)
			m_configurations[it.first.as<std::string>()] = BuildConfiguration(it.second);
	}
}

BuildSettings::~BuildSettings()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

/**
	@brief Add our flags to the provided set
 */
void BuildSettings::GetFlags(string config, set<BuildFlag>& flags)
{
	//Apply global flags
	for(auto x : m_flags)
		flags.emplace(x);
	
	//and then flags for this config
	if(m_configurations.find(config) != m_configurations.end())
		m_configurations[config].GetFlags(flags);
}

void BuildSettings::GetConfigNames(set<string>& configs)
{
	for(auto it : m_configurations)
		configs.emplace(it.first);
}
