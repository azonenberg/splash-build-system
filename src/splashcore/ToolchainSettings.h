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

#ifndef ToolchainSettings_h
#define ToolchainSettings_h

/**
	@brief Settings for a particular toolchain
 */
class ToolchainSettings
{
public:
	ToolchainSettings();
	virtual ~ToolchainSettings();
	
	void PurgeConfig(std::string path);
	void LoadConfig(YAML::Node& node, bool recursive, std::string path);
	
	void GetDefaultArchitectures(std::set<std::string>& arches, std::string path);
	void GetConfigNames(std::string path, std::set<std::string>& configs);
	void GetFlags(std::string config, std::string path, std::set<BuildFlag>& flags);
	
protected:
	void GetDefaultArchitectures_helper(const BuildSettings& settings, std::set<std::string>& arches);

	void SegmentPath(std::string path, std::list<std::string>& dirs);
	std::string GetBuildScriptPath(std::string dir);

	/**
		@brief Map from file paths to BuildSettings declared in that path. Inherited by subdirectories.
	 */
	std::map<std::string, BuildSettings> m_recursiveSettings;
	
	/**
		@brief Map from file paths to BuildSettings declared in that path. Not inherited by subdirectories.
	 */
	std::map<std::string, BuildSettings> m_fileSettings;
};

#endif


