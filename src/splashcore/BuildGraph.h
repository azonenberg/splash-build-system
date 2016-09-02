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

#ifndef BuildGraph_h
#define BuildGraph_h

//TODO: replace this with something that logs to the BuildGraph and displays it clientside
#define LogParseError(...) LogError(__VA_ARGS__)

class BuildGraphNode;

/**
	@brief A DAG of buildable objects
	
	Not even remotely thread safe yet.
	TODO: see if we need to be, or if we can run everything in the client thread
 */
class BuildGraph
{
public:
	BuildGraph();
	virtual ~BuildGraph();

	void UpdateScript(std::string path, std::string hash);
	void RemoveScript(std::string path);

	/**
		@brief Look up the settings object for a particular toolchain
	 */
	ToolchainSettings& GetSettingsForToolchain(std::string chain)
	{ return m_toolchainSettings[chain]; }

protected:
	void Rebuild();
	void InternalRemove(std::string path);
	
	void ParseScript(const std::string& script, std::string path);
	void LoadYAMLDoc(YAML::Node& doc, std::string path);
	
	void LoadConfig(YAML::Node& node, bool recursive, std::string path);
	void LoadTarget(YAML::Node& node, std::string name, std::string path);

	//Log of configuration error messages (displayed to client when we try to build, if not empty)

	//Map of toolchain names to settings for that toolchain
	std::map<std::string, ToolchainSettings> m_toolchainSettings;

	//Our targets

	//The nodes
	std::unordered_set<BuildGraphNode*> m_nodes;
};

#endif
