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

BuildGraph::BuildGraph(WorkingCopy* wc)
	: m_workingCopy(wc)
{
	//TODO: make this configurable in the root Splashfile or something?
	m_buildArtifactPath = "build";
	m_sysIncludePath = "__sysinclude__";
}

BuildGraph::~BuildGraph()
{
	LogDebug("Destroying build graph\n");

	//Delete all targets
	for(auto it : m_targets)
		delete it.second;
	m_targets.clear();

	//Delete all nodes (in no particular order)
	for(auto it : m_nodesByHash)
		delete it.second;
	m_nodesByHash.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

/**
	@brief Check if we have a node with a given hash
 */
bool BuildGraph::HasNodeWithHash(string hash)
{
	lock_guard<recursive_mutex> lock(m_mutex);
	return m_nodesByHash.find(hash) != m_nodesByHash.end();
}

/**
	@brief Find the node with a given hash
 */
BuildGraphNode* BuildGraph::GetNodeWithHash(string hash)
{
	lock_guard<recursive_mutex> lock(m_mutex);
	return m_nodesByHash[hash];
}

/**
	@brief Get a set of all targets, by name, de-duplicated by carch.
 */
void BuildGraph::GetTargets(set<string>& targets)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	//We want a list of all targets that exist for any architecture etc, without duplicates
	//Have to loop over all target nodes then deduplicate with the set.
	for(auto tm : m_targets)
	{
		for(auto it : *tm.second)
			targets.emplace(it.first);
	}
}

/**
	@brief Get the script that a given target was declared in
 */
string BuildGraph::GetTargetScript(string name)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	for(auto tm : m_targets)
	{
		auto m = *tm.second;
		if(m.find(name) == m.end())
			continue;
		return m[name]->GetScript();
	}

	return "";
}

/**
	@brief Get the toolchain that a given target uses
 */
string BuildGraph::GetTargetToolchain(string name)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	for(auto tm : m_targets)
	{
		auto m = *tm.second;
		if(m.find(name) == m.end())
			continue;
		return m[name]->GetToolchain();
	}

	return "";
}

/**
	@brief Get a set of all configs, by name, de-duplicated by arch.
 */
void BuildGraph::GetConfigs(set<string>& configs)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	for(auto it : m_targets)
		configs.emplace(it.first.second);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Garbage collection and target helper stuff

BuildGraph::TargetMap& BuildGraph::GetTargetMap(ArchConfig config)
{
	lock_guard<recursive_mutex> lock(m_mutex);
	if(m_targets.find(config) != m_targets.end())
		return *m_targets[config];

	return *(m_targets[config] = new TargetMap);
}

/**
	@brief Find the node with a given path
 */
BuildGraphNode* BuildGraph::GetNodeWithPath(std::string fname)
{
	lock_guard<recursive_mutex> lock(m_mutex);
	auto hash = m_workingCopy->GetFileHash(fname);
	return m_nodesByHash[hash];
}

/**
	@brief Simple mark-and-sweep garbage collector to remove nodes we no longer have any references to

	(for example, after deleting a dependency)
 */
void BuildGraph::CollectGarbage()
{
	lock_guard<recursive_mutex> lock(m_mutex);

	//LogDebug("Collecting garbage\n");

	//First pass: Mark all nodes unreferenced
	//LogDebug("    Marking %zu nodes as unreferenced\n", m_nodes.size());
	for(auto it : m_nodesByHash)
		it.second->SetUnref();

	//Second pass: Mark all of our targets as referenced.
	//They will propagate the reference flag as needed.
	//TODO: Mark tests as referenced (if tests are not the same as targets)
	//LogDebug("    Marking architectures for %zu targets as referenced\n", m_targets.size());
	for(auto it : m_targets)
	{
		//LogDebug("        Marking %zu targets for architecture %s as referenced\n", it.second->size(), it.first.c_str());
		for(auto jt : *it.second)
		{
			//LogDebug("            Marking nodes for target %s as referenced\n", jt.first.c_str());
			jt.second->SetRef();
		}
	}

	//Third pass: Make a list (by hash) of nodes to delete (can't delete them during iteration)
	list<std::string> garbage;
	//LogDebug("    Scanning for unreferenced nodes\n");
	for(auto it : m_nodesByHash)
	{
		if(!it.second->IsReferenced())
			garbage.push_back(it.first);
	}
	//LogDebug("        Found %d nodes\n", garbage.size());

	//Final step: actually delete them
	//LogDebug("    Deleting unreferenced nodes\n");
	for(auto hash : garbage)
	{
		delete m_nodesByHash[hash];
		m_nodesByHash.erase(hash);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Script parsing

/**
	@brief Updates a build script

	@param path			Relative path of the script
	@param hash			Cache ID of the new script content
	@param body			True if we should scan the body (file_config is included)
	@param config		True if we should scan the recursive_config section
 */
void BuildGraph::UpdateScript(string path, string hash, bool body, bool config)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	//This is now a known script, keep track of it
	m_buildScriptPaths[path] = hash;

	//Reload the build script (and its dependencies)
	InternalUpdateScript(path, hash, body, config);

	//Rebuild the graph to fix up dependencies and delete orphaned nodes
	Rebuild();
}

/**
	@brief Reloads a build script
 */
void BuildGraph::InternalUpdateScript(string path, string hash, bool body, bool config)
{
	//Delete all targets/tests declared in the file
	InternalRemove(path);

	//Read the new script and execute it
	//Don't check if the file is in cache already, it was just updated and is thus LRU
	ParseScript(g_cache->ReadCachedFile(hash), path, body, config);

	//If we changed a script in a parent directory, go through all of our subdirectories and re-parse them
	//since recursively inherited configuration may have changed.
	//TODO: index somehow rather than having to do O(n) search of all known build scripts?
	//TODO: can we patch the configs in at run time somehow rather than re-running it?
	if(config)
	{
		string dir = GetDirOfFile(path);
		for(auto it : m_buildScriptPaths)
		{
			//Path must have our path as a substring, but not be the same script
			string s = it.first;
			if( (s.find(dir) != 0) || (s == path) )
				continue;

			LogDebug("    Build script %s needs to be re-run to reflect changed recursive configurations\n",
				s.c_str());

			InternalUpdateScript(s, m_buildScriptPaths[s], body, config);
		}
	}
}

/**
	@brief Deletes a build script from the working copy

	@param path		Relative path of the script
 */
void BuildGraph::RemoveScript(string path)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	//This is no longer a known script
	m_buildScriptPaths.erase(path);

	//Delete all targets/tests declared in the file
	InternalRemove(path);

	//Rebuild the graph to fix up dependencies and delete orphaned nodes
	Rebuild();
}

/**
	@brief Deletes all targets and tests declared in a given build script
 */
void BuildGraph::InternalRemove(string path)
{
	//Remove any recursive toolchain configurations declared in this file
	for(auto x : m_toolchainSettings)
		x.second.PurgeConfig(path);

	//Find targets declared in that script
	//LogDebug("Purging all nodes declared in %s\n", path.c_str());
	for(auto target : m_targetOrigins[path])
	{
		for(auto it : m_targets)
			it.second->erase(target);

		m_targetReverseOrigins.erase(target);
	}
	m_targetOrigins.erase(path);

	//GC unused nodes
	CollectGarbage();

	//TODO: delete tests

	//Remove references to the script itself
	m_targetOrigins.erase(path);
}

/**
	@brief Loads and executes the YAML commands in the supplied script

	@param script		Script content
	@param path			Relative path of the script (for error messages etc)
	@param body			True if we should scan the body (file_config is included)
	@param config		True if we should scan the recursive_config section
 */
void BuildGraph::ParseScript(const string& script, string path, bool body, bool config)
{
	//LogDebug("Loading build script %s\n", path.c_str());

	try
	{
		//Read the root node
		vector<YAML::Node> nodes = YAML::LoadAll(script);
		for(auto node : nodes)
			LoadYAMLDoc(node, path, body, config);
	}
	catch(YAML::ParserException exc)
	{
		LogParseError("YAML parsing failed: %s\n", exc.what());
	}
}

/**
	@brief Loads and executes the YAML commands in a single document within a build script

	@param doc			The document within the build script
	@param path			Relative path of the script (for error messages etc)
	@param body			True if we should scan the body (file_config is included)
	@param config		True if we should scan the recursive_config section
 */
void BuildGraph::LoadYAMLDoc(YAML::Node& doc, string path, bool body, bool config)
{
	for(auto it : doc)
	{
		string name = it.first.as<std::string>();

		//Configuration stuff is special
		if(name == "recursive_config")
		{
			if(config)
				LoadConfig(it.second, true, path);
		}

		//file_config
		else if(name == "file_config")
		{
			if(body)
				LoadConfig(it.second, false, path);
		}

		//Nope, just a target
		else if(body)
			LoadTarget(it.second, name, path);
	}
}

/**
	@brief Loads configuration for file or recursive scope
 */
void BuildGraph::LoadConfig(YAML::Node& node, bool recursive, string path)
{
	//See what toolchain we're configuring
	if(!node["toolchain"])
	{
		LogParseError("Configuration block in build script \"%s\" cannot be loaded as no toolchain was specified",
			path.c_str());
		return;
	}

	//Configure that toolchain
	m_toolchainSettings[node["toolchain"].as<std::string>()].LoadConfig(node, recursive, path);
}

/**
	@brief Adds a node to the graph
 */
void BuildGraph::AddNode(BuildGraphNode* node)
{
	//Add the node
	m_nodesByHash[node->GetHash()] = node;

	//Put it in the working copy
	//Don't re-scan anything though
	m_workingCopy->UpdateFile(node->GetFilePath(), node->GetHash(), false, false);
}

/**
	@brief Loads configuration for a single target
 */
void BuildGraph::LoadTarget(YAML::Node& node, string name, string path)
{
	LogDebug("Processing target %s in file %s\n", name.c_str(), path.c_str());

	//Complain if this target is already declared
	if(m_targetReverseOrigins.find(name) != m_targetReverseOrigins.end())
	{
		LogParseError(
			"Target \"%s\" in build script \"%s\" conflicts with another target of the same name declared in "
			"build script \"%s\"\n",
			name.c_str(),
			path.c_str(),
			m_targetReverseOrigins[name].c_str());
		return;
	}

	//Sanity check that we asked for a toolchain
	if(!node["toolchain"])
	{
		LogParseError("Target \"%s\" in build script \"%s\" cannot be loaded as no toolchain was specified",
			name.c_str(), path.c_str());
		return;
	}
	string toolchain = node["toolchain"].as<std::string>();

	//Get the toolchain type
	string chaintype;
	size_t offset = toolchain.find("/");
	if(offset != string::npos)
		chaintype = toolchain.substr(0, offset);
	if(chaintype.empty())
	{
		LogParseError("Malformed toolchain name \"%s\" in build script \"%s\"", toolchain.c_str(), path.c_str());
		return;
	}

	//See if we asked for a target type
	string type;
	if(node["type"])
		type = node["type"].as<std::string>();

	//See what architecture(s) we're targeting.
	//Start by pulling in the default architectures
	set<string> darches;
	GetDefaultArchitecturesForToolchain(toolchain, path, darches);

	//Then look to see if we overrode them
	unordered_set<string> arches;
	if(node["arches"])
	{
		auto oarch = node["arches"];

		for(auto it : oarch)
		{
			//If we got a "global" then copy the global arches
			string arch = it.as<std::string>();
			if(arch == "global")
			{
				for(auto a : darches)
					arches.emplace(a);
			}

			//nope, just copy this one
			else
				arches.emplace(arch);
		}
	}

	//See what configurations we might be building for
	set<string> configs;
	GetConfigNames(toolchain, path, configs);

	//Figure out what kind of target we're creating
	//Empty type is OK, pick a reasonable default for that.
	//We need separate nodes for each architecture since they can have different dependencies etc
	for(auto a : arches)
	{
		for(auto c : configs)
		{
			BuildGraphNode* target = NULL;

			//Look up the output path.
			//If none could be determined, skip this target for now
			string exepath = GetOutputFilePath(toolchain, c, a, type, name);
			if(exepath == "")
				continue;

			//C/C++ executables
			if(chaintype == "c++")
			{
				if( (type == "exe") || type.empty() )
					target = new CPPExecutableNode(this, a, c, name, path, exepath, toolchain, node);
			}

			else
			{
				LogParseError("Don't know what to do with toolchain type \"%s\"", chaintype.c_str());
				continue;
			}

			if(target == NULL)
			{
				LogParseError("No target could be created for architecture %s\n", a.c_str());
				continue;
			}

			//Add to target list plus global node set
			GetTargetMap(ArchConfig(a, c))[name] = target;
			AddNode(target);

			//DEBUG: Dump it
			LogDebug("    Target created, dumping dependency tree\n");
			target->PrintInfo(1);
		}
	}

	//Record that we were declared in this file
	m_targetOrigins[path].emplace(name);
	m_targetReverseOrigins[name] = path;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Settings manipulation

/**
	@brief Get the default architecture list for a given toolchain and scope

	@param toolchain	The name of the toolchain to query
	@param path			Path to the build script of the current scope
	@param arches		Set of architectures we found
 */
void BuildGraph::GetDefaultArchitecturesForToolchain(string toolchain, string path, set<string>& arches)
{
	lock_guard<recursive_mutex> lock(m_mutex);
	m_toolchainSettings[toolchain].GetDefaultArchitectures(arches, path);
}

/**
	@brief Gets the full set of configurations which we might be building for

	@param toolchain	The name of the toolchain to query
	@param path			Path to the build script of the current scope
	@param configs		Set of configurations we found
 */
void BuildGraph::GetConfigNames(string toolchain, string path, set<string>& configs)
{
	lock_guard<recursive_mutex> lock(m_mutex);
	m_toolchainSettings[toolchain].GetConfigNames(path, configs);
}

/**
	@brief Gets the flags to be used for a particular architecture and configuration

	@param toolchain	The toolchain in use
	@param config		The configuration to query
	@param path			Path to the build script of the current scope
	@param flags		The set of flags. Any flags already in this set are kept.
 */
void BuildGraph::GetFlags(string toolchain, string config, string path, set<BuildFlag>& flags)
{
	lock_guard<recursive_mutex> lock(m_mutex);
	m_toolchainSettings[toolchain].GetFlags(config, path, flags);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Graph rebuilds

/**
	@brief Rebuilds all topology

	We need to be able to do incremental rebuilds of the graph to avoid redoing everything when a single build script
	changes (like the monstrosity that was Splash v0.1 did).

	Basic rebuild flow is as follows:
	TODO
 */
void BuildGraph::Rebuild()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Filesystem

/**
	@brief Gets the output file name for a *target*.

	Overall path looks like this:
	output_dir/architecture/config/name.suffix

	Returns the empty string if the configuration requested cannot be satisfied with any available toolchain.
 */
string BuildGraph::GetOutputFilePath(
	string toolchain,
	string config,
	string arch,
	string type,
	string name)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	string path = m_buildArtifactPath + "/";
	path += arch + "/";
	path += config + "/";
	path += name;

	//Look up a toolchain to query
	lock_guard<NodeManager> lock2(*g_nodeManager);
	Toolchain* chain = g_nodeManager->GetAnyToolchainForName(arch, toolchain);
	if(chain == NULL)
	{
		LogParseError("Could not find a toolchain of type %s targeting architecture arch %s\n",
			toolchain.c_str(), arch.c_str());
		return "";
	}

	//Figure out the suffix
	if(type == "exe")
		path += chain->GetExecutableSuffix();
	else if(type == "shlib")
		path += chain->GetSharedLibrarySuffix();
	else if(type == "stlib")
		path += chain->GetStaticLibrarySuffix();
	else
		LogParseError("Unknown type \"%s\"\n", type.c_str());

	return path;
}

/**
	@brief Gets the output file name for an intermediate file.

	Overall path looks like this:
	output_dir/architecture/config/original_path/original_basename.suffix

	Returns the empty string if the configuration requested cannot be satisfied with any available toolchain.
 */
string BuildGraph::GetIntermediateFilePath(
	string toolchain,
	string config,
	string arch,
	string type,
	string srcpath)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	string path = m_buildArtifactPath + "/";
	path += arch + "/";
	path += config + "/";

	path += GetDirOfFile(srcpath);
	path += "/";
	path += GetBasenameOfFileWithoutExt(srcpath);

	//Look up a toolchain to query
	lock_guard<NodeManager> lock2(*g_nodeManager);
	Toolchain* chain = g_nodeManager->GetAnyToolchainForName(arch, toolchain);
	if(chain == NULL)
	{
		LogParseError("Could not find a toolchain of type %s targeting architecture arch %s\n",
			toolchain.c_str(), arch.c_str());
		return "";
	}

	//Add the type
	if(type == "object")
		path += chain->GetObjectSuffix();
	else
		LogParseError("Unknown type \"%s\"\n", type.c_str());

	return path;
}
