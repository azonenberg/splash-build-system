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
class WorkingCopy;

/**
	@brief A DAG of buildable objects
	
	TODO: eventually consider having some of the graph nodes (source files etc) be shared across working copies
	to reduce memory usage in large projects
 */
class BuildGraph
{
public:
	BuildGraph(WorkingCopy* wc);
	virtual ~BuildGraph();

	void UpdateScript(std::string path, std::string hash, bool body, bool config);
	void RemoveScript(std::string path);

	/**
		@brief Gets the working copy.

		No mutexing needed as this pointer cannot change once the object is created.
	 */
	WorkingCopy* GetWorkingCopy()
	{ return m_workingCopy; }

	bool HasNodeWithHash(std::string hash);
	BuildGraphNode* GetNodeWithHash(std::string hash);
	BuildGraphNode* GetNodeWithPath(std::string fname);

	void AddNode(BuildGraphNode* node);

	void GetFlags(std::string toolchain, std::string config, std::string path, std::set<BuildFlag>& flags);

	std::string GetOutputFilePath(
		std::string toolchain,
		std::string config,
		std::string arch,
		std::string type,
		std::string name);

	std::string GetIntermediateFilePath(
		std::string toolchain,
		std::string config,
		std::string arch,
		std::string type,
		std::string srcpath);

	void GetTargets(std::set<std::string>& targets);
	std::string GetTargetScript(std::string name);
	std::string GetTargetToolchain(std::string name);

	void GetArches(std::set<std::string>& arches);
	void GetConfigs(std::set<std::string>& configs);

protected:
	void Rebuild();
	void InternalRemove(std::string path);
	void InternalUpdateScript(std::string path, std::string hash, bool body, bool config);
	
	void ParseScript(const std::string& script, std::string path, bool body, bool config);
	void LoadYAMLDoc(YAML::Node& doc, std::string path, bool body, bool config);
	
	void LoadConfig(YAML::Node& node, bool recursive, std::string path);
	void LoadTarget(YAML::Node& node, std::string name, std::string path);

	void CollectGarbage();

	void GetConfigNames(
		std::string toolchain,
		std::string path,
		std::set<std::string>& configs);
	
	void GetDefaultArchitecturesForToolchain(
		std::string toolchain,
		std::string path,
		std::set<std::string>& arches);

	//Our mutex (need to be able to lock when already locked)
	std::recursive_mutex m_mutex;

	//The working copy of the repository we're attached to (so we can access file content etc)
	WorkingCopy* m_workingCopy;
		
	//Build scripts that we know about
	//Map from path to hash
	std::map<std::string, std::string> m_buildScriptPaths;

	//Track where targets were declared
	//Map from path to target names (architecture doesn't matter)
	typedef std::unordered_set<std::string> TargetSet;
	std::map<std::string, TargetSet > m_targetOrigins;

	//Reverse map of target origins (used to catch multiple declarations)
	//Map from target name to script path
	std::map<std::string, std::string> m_targetReverseOrigins;

	//TODO: Log of configuration error messages (displayed to client when we try to build, if not empty)

	//Map of toolchain names to settings for that toolchain
	std::map<std::string, ToolchainSettings> m_toolchainSettings;

	//Map of target names to targets for a single architecture
	typedef std::map<std::string, BuildGraphNode*> TargetMap;

	//An <architecture, config> tuple.
	//There can be at most one target of a given name per ArchConfig.
	typedef std::pair<std::string, std::string> ArchConfig;

	//Map of architecture-config pairs to target sets
	std::map<ArchConfig, TargetMap*> m_targets;

	//helper to create stuff
	TargetMap& GetTargetMap(ArchConfig config);

	//The nodes (map from hash to pointer)
	std::map<std::string, BuildGraphNode*> m_nodesByHash;

	//Relative path for build artifacts to go
	//(also used as logical path for temporary files the client never sees)
	std::string m_buildArtifactPath;

	//Relative path for system include files
	//One directory per toolchain (TODO: use hash instead of version+triplet?)
	std::string m_sysIncludePath;
};

#endif
