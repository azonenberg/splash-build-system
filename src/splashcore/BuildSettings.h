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

#ifndef BuildSettings_h
#define BuildSettings_h

#include "BuildConfiguration.h"

/**
	@brief Build settings declared in a particular scope
	
	Note that these settings are hierarchial, so settings can be inherited from parent scopes.
	
	The WorkingCopy class is responsible for determining the full settings that apply to a given target.
 */
class BuildSettings
{
public:
	BuildSettings();
	BuildSettings(YAML::Node& node);
	
	virtual ~BuildSettings();
	
	const std::unordered_set<BuildFlag>& GetFlags() const
	{ return m_flags; }
	
	bool InheritFlags() const
	{ return m_bInheritFlags; }
	
	const std::unordered_set<std::string>& GetTriplets() const
	{ return m_triplets; }
	
	bool InheritTriplets() const
	{ return m_bInheritTriplets; }

	void GetConfigNames(std::unordered_set<std::string>& configs);

	void GetFlags(std::string config, std::unordered_set<BuildFlag>& flags);
		
protected:

	/**
		@brief True if we inherit global triplets
	 */
	bool m_bInheritTriplets;


	/**
		@brief Architecture triplets
	 */
	std::unordered_set<std::string> m_triplets;
	
	/**
		@brief True if we inherit global flags
	 */
	bool m_bInheritFlags;

	/**
		@brief Flags that apply regardless of configuration
	 */
	std::unordered_set<BuildFlag> m_flags;

	/**
		@brief Map of configuration names to configuration objects
	 */
	std::map<std::string, BuildConfiguration> m_configurations;
};

#endif

