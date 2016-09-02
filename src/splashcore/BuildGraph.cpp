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

BuildGraph::BuildGraph()
{

}

BuildGraph::~BuildGraph()
{
	//Delete all nodes (in no particular order)
	for(auto x : m_nodes)
		delete x;
	m_nodes.clear();
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
	for(auto x : m_toolchainSettings)
		x.second.PurgeConfig(path);
	
	//TODO: delete targets
	//TODO: delete tests
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
	LogDebug("    Loading configuration\n");
	
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
	LogDebug("    Loading target %s\n", name.c_str());

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
	unordered_set<string> arches;
	GetDefaultArchitecturesForToolchain(toolchain, path, arches);
	for(auto a : arches)
		LogDebug("        Default arch: %s\n", a.c_str());
		
	//Then look to see if we overrode them
	if(node["arches"])
	{
		auto oarch = node["arches"];
		
		//See if we have a global declaration
		//TODO: is there a faster way to do this (w/o two traversals)?
		bool haveGlobal = false;
		for(auto it : oarch)
		{
			if(it.as<std::string>() == "global")
				haveGlobal = true;
		}
		
		if(haveGlobal)
			LogDebug("have global\n");
		else
			LogDebug("do not have global\n");
	}

	//Figure out what kind of target we're creating
	//Empty type is OK, pick a reasonable default for
	BuildGraphNode* target = NULL;
	if(chaintype == "cpp")
	{
		if( (type == "exe") || type.empty() )
		{
			LogDebug("        Target is a C++ executable\n");
		}
	}
	else
	{
		LogParseError("Don't know what to do with toolchain type \"%s\"", chaintype.c_str());
		return;
	}
	
	//TODO: Add to target list
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
