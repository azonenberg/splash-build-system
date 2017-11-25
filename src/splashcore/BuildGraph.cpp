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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

BuildGraph::BuildGraph(WorkingCopy* wc)
	: m_workingCopy(wc)
	, m_buildArtifactPath("build")			//TODO: make this configurable in the root Splashfile or something?
	, m_sysIncludePath("__sysinclude__")
{
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
	@brief Get a set of all arches, by name, de-duplicated by config.
 */
void BuildGraph::GetArches(set<string>& arches)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	for(auto it : m_targets)
		arches.emplace(it.first.first);
}

/**
	@brief Get a set of all arches, by name, for a specific target
 */
void BuildGraph::GetArches(set<string>& arches, string target)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	for(auto it : m_targets)
	{
		auto m = *it.second;
		if(m.find(target) == m.end())
			continue;

		arches.emplace(it.first.first);
	}
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

/**
	@brief Get a set of all graph nodes
 */
void BuildGraph::GetNodes(set<string>& nodes)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	for(auto it : m_nodesByHash)
		nodes.emplace(it.first);
}

/**
	@brief Checks if the requested target exists (for some arch/config combination)
 */
bool BuildGraph::HasTarget(string target)
{
	for(auto it : m_targets)
	{
		TargetMap& cm = *it.second;
		for(auto jt : cm)
		{
			if(jt.first == target)
				return true;
		}
	}

	return false;
}

/**
	@brief Get all targets matching the requested filter

	If libs_too is set, finds all targets that matched targets link to as well
 */
void BuildGraph::GetTargets(set<BuildGraphNode*>& nodes, string target, string arch, string config, bool libs_too)
{
	//if(libs_too)
	//	LogTrace("GetTargets with libs\n");

	for(auto it : m_targets)
	{
		//Skip if we don't have a match
		//(note that empty filter = match everything
		auto ca = it.first;
		if( (ca.second != config) && !config.empty() )
			continue;
		if( (ca.first != arch) && !arch.empty() )
			continue;

		//Check our targets
		TargetMap& cm = *it.second;
		for(auto jt : cm)
		{
			if( (jt.first != target) && !target.empty() )
				continue;

			//This is a match!
			auto node = jt.second;
			nodes.emplace(node);

			//If we're looking for libraries, pull in all of the libs that this node links to
			if(libs_too)
				GetLibrariesForTarget(node, nodes);
		}
	}
}

/**
	@brief Recursively find all targets that the specified node links to
 */
void BuildGraph::GetLibrariesForTarget(BuildGraphNode* target, set<BuildGraphNode*>& nodes)
{
	//We don't look at *flags* since the node is an executable
	//We need to look at the source files to the linker, aka objects and libs!
	auto& sources = target->GetSources();
	for(auto src : sources)
	{
		auto node = GetNodeWithPath(src);

		//If we already have this node in the list, don't look further and don't recurse
		if(nodes.find(node) != nodes.end())
			continue;

		//If this node is not a target, we don't care
		auto name = node->GetName();
		ArchConfig ac(node->GetArch(), node->GetConfig());
		if(m_targets.find(ac) == m_targets.end())
			continue;
		TargetMap& tm = *m_targets[ac];
		if(tm.find(name) == tm.end())
			continue;
		if(tm[name] != node)
			continue;

		//New target - add to list and recurse
		nodes.emplace(node);
		GetLibrariesForTarget(target, nodes);
	}
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
	if(hash == "")
	{
		LogWarning("BuildGraph: path %s has no hash\n", fname.c_str());
		return NULL;
	}

	if(m_nodesByHash.find(hash) == m_nodesByHash.end())
	{
		LogWarning("BuildGraph: hash %s (filename %s) is not in graph\n", hash.c_str(), fname.c_str());
		//asm("int3");
		return NULL;
	}

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
	//LogIndenter li;

	//First pass: Mark all nodes unreferenced
	//LogDebug("    Marking %zu nodes as unreferenced\n", m_nodesByHash.size());
	for(auto it : m_nodesByHash)
		it.second->SetUnref();

	//Second pass: Mark all of our targets as referenced.
	//They will propagate the reference flag as needed.
	//TODO: Mark tests as referenced (if tests are not the same as targets)
	//LogDebug("    Marking architectures for %zu targets as referenced\n", m_targets.size());
	for(auto it : m_targets)
	{
		//LogDebug("        Marking %zu targets for architecture %s as referenced\n",
		//	it.second->size(), it.first.second.c_str());
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
void BuildGraph::UpdateScript(string path, string hash, bool body, bool config, set<string>& dirtyScripts)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	LogDebug("Updating build script %s (body=%d, config=%d)\n", path.c_str(), body, config);
	//LogIndenter li;

	//This is now a known script, keep track of it
	m_buildScriptPaths[path] = hash;

	//Reload the build script (and its dependencies)
	InternalUpdateScript(path, hash, body, config, dirtyScripts);
}

/**
	@brief Reloads a build script
 */
void BuildGraph::InternalUpdateScript(string path, string hash, bool body, bool config, set<string>& dirtyScripts)
{
	//Delete all targets/tests declared in the file
	InternalRemove(path);

	//Read the new script and execute it
	//Don't check if the file is in cache already, it was just updated and is thus LRU
	ParseScript(g_cache->ReadCachedFile(hash), path, body, config, dirtyScripts);

	//If we changed a script in a parent directory, go through all of our subdirectories and re-parse them
	//since recursively inherited configuration may have changed.
	//TODO: index somehow rather than having to do O(n) search of all known build scripts?
	//TODO: can we patch the configs in at run time somehow rather than re-running it?
	if(config)
	{
		LogIndenter li;

		string dir = GetDirOfFile(path);
		for(auto it : m_buildScriptPaths)
		{
			//Path must have our path as a substring, but not be the same script
			string s = it.first;
			size_t pos = s.find(dir);
			if( (pos != 0) || (s == path) )
				continue;

			//LogDebug("Build script %s needs to be re-run to reflect changed recursive configurations\n",
			//	s.c_str());
			//LogDebug("pos=%zu dir=%s path=%s\n", pos, dir.c_str(), path.c_str());

			InternalUpdateScript(s, m_buildScriptPaths[s], body, config, dirtyScripts);
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

	//TODO: delete tests

	//Remove references to the script itself
	m_targetOrigins.erase(path);

	//Remove dependencies of the script
	for(auto& it : m_sourceFileDependencies)
		it.second.erase(path);
}

/**
	@brief Loads and executes the YAML commands in the supplied script

	@param script		Script content
	@param path			Relative path of the script (for error messages etc)
	@param body			True if we should scan the body (file_config is included)
	@param config		True if we should scan the recursive_config section
	@param dirtyScripts	Set of scripts that might have to be rebuilt as a result of this one changing
 */
void BuildGraph::ParseScript(const string& script, string path, bool body, bool config, set<string>& dirtyScripts)
{
	//LogDebug("Loading build script %s\n", path.c_str());

	try
	{
		//Read the root node
		vector<YAML::Node> nodes = YAML::LoadAll(script);
		for(auto node : nodes)
			LoadYAMLDoc(node, path, body, config, dirtyScripts);
	}
	catch(YAML::ParserException& exc)
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
	@param dirtyScripts	Set of scripts that might have to be rebuilt as a result of this one changing
 */
void BuildGraph::LoadYAMLDoc(YAML::Node& doc, string path, bool body, bool config, set<string>& dirtyScripts)
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
		{
			LoadTarget(it.second, name, path);

			auto& targets = GetDependentScripts(name);
			for(auto t : targets)
			{
				LogDebug("Dirty script %s added (from target %s)\n", t.c_str(), name.c_str());
				dirtyScripts.emplace(t);
			}
		}
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
	m_toolchainSettings[node["toolchain"].as<string>()].LoadConfig(node, recursive, path);
}

/**
	@brief Adds a node to the graph
 */
void BuildGraph::AddNode(BuildGraphNode* node)
{
	//LogDebug("    Adding node %s to %p (%d nodes so far)\n", node->GetHash().c_str(), this, m_nodesByHash.size());

	//Add the node
	m_nodesByHash[node->GetHash()] = node;

	//Put it in the working copy
	//Don't re-scan anything though
	set<string> ignored;
	m_workingCopy->UpdateFile(node->GetFilePath(), node->GetHash(), false, false, ignored);
}

/**
	@brief Loads configuration for a single target
 */
void BuildGraph::LoadTarget(YAML::Node& node, string name, string path)
{
	LogDebug("Processing target %s in file %s\n", name.c_str(), path.c_str());
	LogIndenter li;

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
		LogParseError("Target \"%s\" in build script \"%s\" cannot be loaded as no toolchain was specified\n",
			name.c_str(), path.c_str());
		return;
	}
	string toolchain = node["toolchain"].as<std::string>();

	//If we asked for constant tables, generate them
	if(node["constants"])
		ProcessConstantTables(node["constants"], path);

	//Get the toolchain type
	string chaintype;
	size_t offset = toolchain.find("/");
	if(offset != string::npos)
		chaintype = toolchain.substr(0, offset);
	if(chaintype.empty())
	{
		LogParseError("Malformed toolchain name \"%s\" in build script \"%s\"\n", toolchain.c_str(), path.c_str());
		return;
	}

	//See if we asked for a target type
	string type;
	if(node["type"])
		type = node["type"].as<std::string>();

	//See what architecture(s) we're targeting.
	//Start by pulling in the default architectures
	set<string> darches;
	unordered_set<string> arches;
	GetDefaultArchitecturesForToolchain(toolchain, path, darches);

	//If we specify a list of target boards, read the BSP rather than the arch list
	BoardInfoFile* boardInfo = NULL;
	string infoHash;
	map<string, BoardInfoFile*> boards;
	if(node["boards"])
	{
		//If we also specified arches something derpy is going on... complain!
		if(node["arches"])
		{
			LogParseError("Node \"%s\" specified both boards and arches in build script \"%s\"\n",
				name.c_str(), path.c_str());
			return;
		}

		//Read the board list and crunch that
		auto nboards = node["boards"];
		string base = GetDirOfFile(path);
		for(auto it : nboards)
		{
			string fname = it.as<string>();
			string basename = GetBasenameOfFile(fname);

			//Get the YAML file name and make sure we have it
			string bpath = base + "/" + fname;
			if(!CanonicalizePathThatMightNotExist(bpath))
			{
				LogParseError("Board file name \"%s\" in build script \"%s\" is malformed\n",
					bpath.c_str(), path.c_str());
				return;
			}
			if(!m_workingCopy->HasFile(bpath))
			{
				LogParseError("Board file \"%s\" in build script \"%s\" does not exist\n",
					bpath.c_str(), path.c_str());
				return;
			}

			//Load the board info
			infoHash = m_workingCopy->GetFileHash(bpath);
			boards[basename] = new BoardInfoFile(g_cache->ReadCachedFile(infoHash));
		}
	}

	//Look to see if we overrode defaults
	else if(node["arches"])
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

	//Not overriding, copy defaults
	else
	{
		for(auto a : darches)
			arches.emplace(a);
	}

	//See what configurations we might be building for
	set<string> configs;
	GetConfigNames(toolchain, path, configs);

	//If we have boards, process these separately
	if(!boards.empty())
	{
		for(auto it : boards)
		{
			auto bfname = it.first;
			auto board = it.second;

			//Look up the toolchain for the board's architecture triplet
			auto arch = board->GetTriplet();
			if(NULL == g_nodeManager->GetAnyToolchainForName(arch, toolchain))
			{
				LogError(
					"Target \"%s\" (declared in \"%s\") needs a toolchain of type \"%s\" for architecture \"%s\",\n"
					"but no nodes could provide it\n",
					name.c_str(), path.c_str(), toolchain.c_str(), arch.c_str());
				continue;
			}
			LogDebug("Processing design for board %s (arch %s)\n", bfname.c_str(), arch.c_str());

			//Only supported type is verilog/bitstream
			if( (chaintype != "verilog") || (type != "bitstream") )
			{
				LogError("Target types other than verilog/bitstream are not supported for board nodes\n");
				continue;
			}

			//Create a node for each configuration
			auto boardbase = GetBasenameOfFileWithoutExt(bfname);
			for(auto c : configs)
			{
				LogIndenter li;
				auto bitpath = GetOutputFilePath(toolchain, c, arch, type, name, bfname);
				//LogDebug("Output for config %s is %s\n", c.c_str(), bitpath.c_str());

				auto target = new FPGABitstreamNode(this, arch, c, name, path, bitpath, toolchain, bfname, board, node);
				GetTargetMap(ArchConfig(boardbase, c))[name] = target;
				AddNode(target);
			}
		}

		for(auto it : boards)
			delete it.second;
		return;
	}

	//Figure out what kind of target we're creating
	//Empty type is OK, pick a reasonable default for that.
	//We need separate nodes for each architecture since they can have different dependencies etc
	for(auto a : arches)
	{
		//If we don't have any toolchains for this architecture, skip it
		if(NULL == g_nodeManager->GetAnyToolchainForName(a, toolchain))
		{
			LogError(
				"Target \"%s\" (declared in \"%s\") needs a toolchain of type \"%s\" for architecture \"%s\",\n"
				"but no nodes could provide it\n",
				name.c_str(), path.c_str(), toolchain.c_str(), a.c_str());
			continue;
		}

		for(auto c : configs)
		{
			BuildGraphNode* target = NULL;

			//Look up the output path.
			//If none could be determined, skip this target for now
			string exepath = GetOutputFilePath(toolchain, c, a, type, name);
			if(exepath == "")
			{
				LogWarning("exepath for %s, %s, %s, %s, %s is empty\n",
					toolchain.c_str(), c.c_str(), a.c_str(), type.c_str(), name.c_str());
				continue;
			}

			//C/C++ executables
			if(chaintype == "c++")
			{
				if( (type == "exe") || type.empty() )
					target = new CPPExecutableNode(this, a, c, name, path, exepath, toolchain, node);

				else if(type == "shlib")
					target = new CPPSharedLibraryNode(this, a, c, name, path, exepath, toolchain, node);

				else
				{
					LogParseError("Don't know what to do with target type \"%s\"\n", type.c_str());
					continue;
				}
			}

			//Verilog designs
			//TODO: Replace "verilog" with "hdl"? How to handle mixed language designs?
			else if(chaintype == "verilog")
			{
				if(type == "formal")
					target = new FormalVerificationNode(this, a, c, name, path, exepath, toolchain, node);

				else
				{
					LogParseError("Don't know what to do with target type \"%s\"\n", type.c_str());
					continue;
				}
			}

			//Something is wrong
			else
			{
				LogParseError("Don't know what to do with toolchain type \"%s\"\n", chaintype.c_str());
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
			//LogDebug("    Target created, dumping dependency tree\n");
			//target->PrintInfo(1);
		}
	}

	//Record that we were declared in this file
	m_targetOrigins[path].emplace(name);
	m_targetReverseOrigins[name] = path;

	//Clean up
	if(boardInfo)
	{
		delete boardInfo;
		boardInfo = NULL;
	}
}

/**
	@brief Loads a "constants" declaration in a build script

	@param node		The "constants" node
	@param path		Path to the build script we were in (used for resolving relative paths)s
 */
bool BuildGraph::ProcessConstantTables(const YAML::Node& node, string path)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	for(auto n : node)
	{
		string script_fname = n.first.as<std::string>();
		for(auto g : n.second)
		{
			if(!ProcessConstantTable(path, script_fname, g.as<std::string>()))
				return false;
		}
	}

	return true;
}

/**
	@brief Loads a single table of constants

	The generated file will be in the same directory as the build script which asked for it.

	@param scriptpath	Path to the build script we were in (used for resolving relative paths)s
	@param tablepath	Path to the constant table
	@param generator	Name of the generator to use
 */
bool BuildGraph::ProcessConstantTable(string scriptpath, string tablepath, string generator)
{
	//If it begins with a /, it's an absolute path (relative to project root)
	string rtpath;
	if(tablepath[0] == '/')
		rtpath = tablepath.substr(1);

	//Assume relative path
	else
		rtpath = GetDirOfFile(scriptpath) + "/" + tablepath;

	//Either way we have to canonicalize it
	if(!CanonicalizePathThatMightNotExist(rtpath))
	{
		LogError("Couldn't canonicalize constant table (bad path?)\n");
		return false;
	}
	//LogDebug("Logical table path %s\n", rtpath.c_str());

	//Create source file node for the table if it didn't already exist
	if(!m_workingCopy->HasFile(rtpath))
	{
		LogError("Couldn't find constant table %s (nonexistent table filename?)\n", rtpath.c_str());
		return false;
	}
	string hash = m_workingCopy->GetFileHash(rtpath);
	if(!HasNodeWithHash(hash))
		AddNode(new SourceFileNode(this, rtpath, hash));
	//LogDebug("Table has hash %s\n", hash.c_str());

	//Look up the content of the file so we can crunch it
	string table_yaml = g_cache->ReadCachedFile(hash);

	//Read the root node
	vector<YAML::Node> nodes = YAML::LoadAll(table_yaml);
	set<string> ignored;
	for(auto doc : nodes)
	{
		for(auto it : doc)
		{
			//Figure out the output filename
			string name = it.first.as<std::string>();
			string opath = GetDirOfFile(scriptpath) + "/" + ConstantTableNode::GetOutputBasename(name, generator);
			LogTrace("Enum %s is at %s\n", name.c_str(), opath.c_str());

			//Create and add the node
			//Do not touch any build scripts.
			auto n = new ConstantTableNode(this, opath, name, it.second, rtpath,
				generator, sha256(table_yaml), scriptpath);
			AddNode(n);
			m_workingCopy->UpdateFile(opath, n->GetHash(), false, false, ignored);
		}
	}

	return true;
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
	if(m_toolchainSettings.find(toolchain) == m_toolchainSettings.end())
		LogWarning("BuildGraph::GetFlags: Don't have any ToolchainSettings for chain %s\n", toolchain.c_str());
	m_toolchainSettings[toolchain].GetFlags(config, path, flags);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Graph rebuilds

/**
	@brief Rebuilds all topology

	We need to be able to do incremental rebuilds of the graph to avoid redoing everything when a single build script
	changes (like the monstrosity that was Splash v0.1 did).
 */
void BuildGraph::Rebuild()
{
	//LogDebug("Rebuilding graph\n");
	LogIndenter li;

	lock_guard<recursive_mutex> lock(m_mutex);

	//Make a set of all of our nodes
	//LogDebug("Building node set\n");
	set<BuildGraphNode*> nodes;
	for(auto it : m_nodesByHash)
		nodes.emplace(it.second);

	//Finalize each node in the graph
	//LogDebug("Finalizing nodes\n");
	for(auto n : nodes)
		n->StartFinalization();
	for(auto n : nodes)
		n->Finalize();

	//Collect any garbage we generated
	CollectGarbage();
}

void BuildGraph::FinalizeCallback(BuildGraphNode* node, string old_hash)
{
	//Update the node to the new hash
	string new_hash = node->GetHash();
	if(old_hash != new_hash)
	{
		m_nodesByHash.erase(old_hash);
		m_nodesByHash[new_hash] = node;
	}

	//Add dependencies to that node's script
	string script = node->GetScript();
	if(script != "")
	{
		auto deps = node->GetDependencies();
		for(auto d : deps)
			m_sourceFileDependencies[d].emplace(script);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Filesystem

/**
	@brief Gets the output file name for a *target*.

	Overall path looks like this:
	output_dir/architecture/config/prefixname.suffix

	Returns the empty string if the configuration requested cannot be satisfied with any available toolchain.
 */
string BuildGraph::GetOutputFilePath(
	string toolchain,
	string config,
	string arch,
	string type,
	string name,
	string board)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	//Look up a toolchain to query
	lock_guard<NodeManager> lock2(*g_nodeManager);
	Toolchain* chain = g_nodeManager->GetAnyToolchainForName(arch, toolchain);
	if(chain == NULL)
	{
		LogParseError("Could not find a toolchain of type %s targeting architecture arch %s\n",
			toolchain.c_str(), arch.c_str());
		return "";
	}
	if(!chain->IsTypeValid(type))
	{
		LogParseError("Unknown type \"%s\"\n", type.c_str());
		return "";
	}

	string path = m_buildArtifactPath + "/";
	if(board != "")
		path += GetBasenameOfFileWithoutExt(board) + "/";
	else
		path += arch + "/";
	path += config + "/";

	//Filename including pre/suffix
	path += chain->GetPrefix(type);
	path += name;
	path += chain->GetSuffix(type);

	LogIndenter li;
	//LogDebug("final path = %s\n", path.c_str());
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
	string srcpath,
	string board)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	//Look up a toolchain to query
	lock_guard<NodeManager> lock2(*g_nodeManager);
	Toolchain* chain = g_nodeManager->GetAnyToolchainForName(arch, toolchain);
	if(chain == NULL)
	{
		LogParseError("Could not find a toolchain of type %s targeting architecture arch %s\n",
			toolchain.c_str(), arch.c_str());
		return "";
	}

	if(!chain->IsTypeValid(type))
	{
		LogParseError("Unknown type \"%s\"\n", type.c_str());
		return "";
	}

	string path = m_buildArtifactPath + "/";
	if(board != "")
		path += GetBasenameOfFileWithoutExt(board) + "/";
	else
		path += arch + "/";
	path += config + "/";

	path += GetDirOfFile(srcpath);
	path += "/";
	path += chain->GetPrefix(type);
	path += GetBasenameOfFileWithoutExt(srcpath);
	path += chain->GetSuffix(type);

	return path;
}
