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

}

BuildGraph::~BuildGraph()
{
	LogDebug("Destroying build graph\n");
	
	//Delete all targets
	for(auto it : m_targets)
		delete it.second;
	m_targets.clear();
	
	//Delete all nodes (in no particular order)
	for(auto x : m_nodes)
		delete x;
	m_nodes.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Garbage collection and target helper stuff

BuildGraph::TargetMap& BuildGraph::GetTargetMap(ArchConfig config)
{
	if(m_targets.find(config) != m_targets.end())
		return *m_targets[config];

	return *(m_targets[config] = new TargetMap);
}

/**
	@brief Simple mark-and-sweep garbage collector to remove nodes we no longer have any references to

	(for example, after deleting a dependency)
 */
void BuildGraph::CollectGarbage()
{
	//LogDebug("Collecting garbage\n");
	
	//First pass: Mark all nodes unreferenced
	//LogDebug("    Marking %zu nodes as unreferenced\n", m_nodes.size());
	for(auto n : m_nodes)
		n->SetUnref();

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

	//Third pass: Make a list of nodes to delete (can't delete them during iteration)
	list<BuildGraphNode*> garbage;
	//LogDebug("    Scanning for unreferenced nodes\n");
	for(auto n : m_nodes)
	{
		if(!n->IsReferenced())
			garbage.push_back(n);
	}
	//LogDebug("        Found %d nodes\n", garbage.size());

	//Final step: actually delete them
	//LogDebug("    Deleting unreferenced nodes\n");
	for(auto n : garbage)
	{
		m_nodes.erase(n);
		delete n;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Script parsing

/**
	@brief Updates a build script

	@param path		Relative path of the script
	@param hash		Cache ID of the new script content
 */
void BuildGraph::UpdateScript(string path, string hash)
{
	//This is now a known script, keep track of it
	m_buildScriptPaths[path] = hash;
	
	//Reload the build script (and its dependencies)
	InternalUpdateScript(path, hash);	

	//Rebuild the graph to fix up dependencies and delete orphaned nodes
	Rebuild();
}

/**
	@brief Reloads a build script
 */
void BuildGraph::InternalUpdateScript(string path, string hash)
{
	//Delete all targets/tests declared in the file
	InternalRemove(path);

	//Read the new script and execute it
	//Don't check if the file is in cache already, it was just updated and is thus LRU
	ParseScript(g_cache->ReadCachedFile(hash), path);
	
	//If we changed a script in a parent directory, go through all of our subdirectories and re-parse them
	//since recursively inherited configuration may have changed.
	//TODO: index somehow rather than having to do O(n) search of all known build scripts?
	//TODO: can we patch the configs in at run time somehow rather than re-running it?
	string dir = GetDirOfFile(path);
	for(auto it : m_buildScriptPaths)
	{		
		//Path must have our path as a substring, but not be the same script
		string s = it.first;
		if( (s.find(dir) != 0) || (s == path) )
			continue;
			
		LogDebug("    Build script %s needs to be re-run to reflect changed recursive configurations\n",
			s.c_str());
			
		InternalUpdateScript(s, m_buildScriptPaths[s]);
	}
}

/**
	@brief Deletes a build script from the working copy

	@param path		Relative path of the script
 */
void BuildGraph::RemoveScript(string path)
{
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
 */
void BuildGraph::ParseScript(const string& script, string path)
{
	//LogDebug("Loading build script %s\n", path.c_str());
	
	try
	{
		//Read the root node
		vector<YAML::Node> nodes = YAML::LoadAll(script);
		for(auto node : nodes)
			LoadYAMLDoc(node, path);
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
 */
void BuildGraph::LoadYAMLDoc(YAML::Node& doc, string path)
{
	for(auto it : doc)
	{
		string name = it.first.as<std::string>();
		
		//Configuration stuff is special
		if(name == "recursive_config")
			LoadConfig(it.second, true, path);
		else if(name == "file_config")
			LoadConfig(it.second, false, path);
			
		//Nope, just a target
		else
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
	unordered_set<string> darches;
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
	unordered_set<string> configs;
	GetConfigNames(toolchain, path, configs);

	//Figure out what kind of target we're creating
	//Empty type is OK, pick a reasonable default for that.
	//We need separate nodes for each architecture since they can have different dependencies etc
	for(auto a : arches)
	{
		for(auto c : configs)
		{
			BuildGraphNode* target = NULL;

			//C/C++ executables
			if(chaintype == "cpp")
			{
				if( (type == "exe") || type.empty() )
					target = new CPPExecutableNode(this, a, c, name, path, toolchain, node);
			}
			
			else
			{
				LogParseError("Don't know what to do with toolchain type \"%s\"", chaintype.c_str());
				return;
			}

			if(target == NULL)
			{
				LogParseError("No target could be created for architecture %s\n", a.c_str());
				return;
			}

			//Add to target list plus global node set
			GetTargetMap(ArchConfig(a, c))[name] = target;
			m_nodes.emplace(target);
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
void BuildGraph::GetDefaultArchitecturesForToolchain(string toolchain, string path, unordered_set<string>& arches)
{
	m_toolchainSettings[toolchain].GetDefaultArchitectures(arches, path);
}

/**
	@brief Gets the full set of configurations which we might be building for

	@param toolchain	The name of the toolchain to query
	@param path			Path to the build script of the current scope
	@param configs		Set of configurations we found
 */
void BuildGraph::GetConfigNames(string toolchain, string path, unordered_set<string>& configs)
{
	m_toolchainSettings[toolchain].GetConfigNames(path, configs);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Graph rebuilds

/**
	@brief Rebuilds all topology

	We need to be able to do incremental rebuilds of the graph to avoid redoing everything when a single build script
	changes (like the monstrosity that was Splash v0.1 did).

	Basic rebuild flow is as follows:

	READ SCRIPT

	GARBAGE COLLECT
	* For all graph nodes
		* Mark as unreferenced
	* Add nodes for all targets and tests to a FIFO
	* While fifo not empty
		* Pop node
		* If node is referenced, go on to next node
		* Mark node as referenced
		* Add all dependencies of the node to the tail of the FIFO
	* For all graph nodes
		* If not referenced, delete it
 */
void BuildGraph::Rebuild()
{
}
