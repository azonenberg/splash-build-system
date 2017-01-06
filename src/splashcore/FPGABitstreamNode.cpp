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

FPGABitstreamNode::FPGABitstreamNode(
	BuildGraph* graph,
	string arch,
	string config,
	string name,
	string scriptpath,
	string path,
	string toolchain,
	YAML::Node& node)
	: BuildGraphNode(graph, BuildFlag::LINK_TIME, toolchain, arch, config, name, scriptpath, path, node)
	, m_scriptpath(scriptpath)
{
	//LogDebug("Creating FPGABitstreamNode (toolchain %s, output fname %s)\n",
	//	toolchain.c_str(), path.c_str());
	//LogIndenter li;
	/*
	//Look up the type of executable we are
	string type;
	if(node["type"])
		type = node["type"].as<std::string>();

	if(type == "shlib")
	{
		//Add the "shared library" flag
		m_flags.emplace(BuildFlag("output/shared"));
	}

	//Sanity check: we must have some source files!
	if(!node["sources"])
	{
		SetInvalidInput(
			string("FPGABitstreamNode: cannot have a C++ executable (") + name + ", declared in " +
			path +") without any source files\n");
		return;
	}
	auto snode = node["sources"];

	//Look up the working copy we're part of
	WorkingCopy* wc = m_graph->GetWorkingCopy();

	//Look up the source files and see if we have source nodes for them yet
	string dir = GetDirOfFile(scriptpath);
	for(auto it : snode)
	{
		//File name is relative to the build script.
		//Get the actual path name (TODO: canonicalize ../ etc)
		string fname = (dir + "/" + it.as<std::string>());

		//Now we can check the working copy and see what the file looks like.
		//Gotta make sure it's there first!
		if(!wc->HasFile(fname))
		{
			SetInvalidInput(string("FPGABitstreamNode: No file named ") + fname + " in working copy\n");
			return;
		}

		//We have the file, look up the hash
		//This is a separate mutex operation from HasFile(), but no real risk of a race condition
		//because the constructor is only called from the DevClientThread, which is the only thread
		//that can remove files from the working copy.
		string hash = wc->GetFileHash(fname);

		//If we already have a node, save it
		if(m_graph->HasNodeWithHash(hash))
		{
			//LogDebug("Already have source node for %s\n", fname.c_str());
			m_sourcenodes.emplace(m_graph->GetNodeWithHash(hash));
			continue;
		}

		//Nope, need to create one
		auto src = new SourceFileNode(m_graph, fname, hash);
		graph->AddNode(src);
		m_sourcenodes.emplace(src);
	}
	*/
}

/**
	@brief Create all of our object files and kick off the dependency scans for them
 */
void FPGABitstreamNode::DoStartFinalization()
{
	/*
	//LogDebug("DoFinalize for %s\n", GetFilePath().c_str());
	//LogIndenter li;

	//Look up the working copy we're part of
	WorkingCopy* wc = m_graph->GetWorkingCopy();

	//Collect the compiler flags
	set<BuildFlag> compileFlags;
	GetFlagsForUseAt(BuildFlag::COMPILE_TIME, compileFlags);

	//We have source nodes. Create the object nodes.
	set<string> ignored;
	for(auto s : m_sourcenodes)
	{
		//Get the output file name
		auto src = s->GetFilePath();
		string fname = m_graph->GetIntermediateFilePath(
			m_toolchain,
			m_config,
			m_arch,
			"object",
			src);

		//Create a test node first to compute the hash.
		//If another node with that hash already exists, delete it and use the old node instead.
		//TODO: more efficient way of doing this? cache dependencies and do simpler parse or something?
		auto obj = new CPPObjectNode(
			m_graph,
			m_arch,
			src,
			fname,
			m_toolchain,
			m_scriptpath,
			compileFlags);

		//If we have a node for this hash already, delete it and use the existing one
		string h = obj->GetHash();
		if(m_graph->HasNodeWithHash(h))
		{
			delete obj;
			obj = dynamic_cast<CPPObjectNode*>(m_graph->GetNodeWithHash(h));
		}

		//It's new, keep it and add to the graph
		else
			m_graph->AddNode(obj);

		//Either way we have the node now. Add to our list of sources.
		m_objects.emplace(obj);
		m_sources.emplace(fname);
		m_dependencies.emplace(fname);

		//Add the object file to our working copy.
		//Don't worry about dirtying targets in other files, there shouldn't be any
		wc->UpdateFile(fname, h, false, false, ignored);
	}
	*/
}

/**
	@brief Calculate our final hash etc
 */
void FPGABitstreamNode::DoFinalize()
{
	/*
	//Update libdeps and libflags
	//TODO: Can we do this per executable, and not separately for each object?
	for(auto obj : m_objects)
		obj->GetLibraryScanResults(m_libdeps, m_libflags);

	//Collect the linker flags
	set<BuildFlag> linkflags;
	GetFlagsForUseAt(BuildFlag::LINK_TIME, linkflags);

	//TODO: The toolchain specified for us is the OBJECT FILE generation toolchain.
	//How do we find the LINKER to use?

	//Add all linker flags OTHER than those linking to a library
	m_flags.clear();
	for(auto f : linkflags)
	{
		if(f.GetType() != BuildFlag::TYPE_LIBRARY)
			m_flags.emplace(f);
	}

	//Go over the set of link flags and see if any of them specify linking to a TARGET.
	//If so, look up that target
	for(auto f : linkflags)
	{
		if(f.GetType() != BuildFlag::TYPE_LIBRARY)
			continue;
		if(f.GetFlag() != "target")
			continue;

		//Look up the target
		set<BuildGraphNode*> nodes;
		m_graph->GetTargets(nodes, f.GetArgs(), m_arch, m_config);
		if(nodes.empty())
		{
			SetInvalidInput(
				string("FPGABitstreamNode: Could not link to target ") + f.GetArgs() + " because it doesn't exist\n");
			return;
		}
		//TODO: verify only one?

		//We found the target, use it
		BuildGraphNode* node = *nodes.begin();
		string path = node->GetFilePath();
		//LogDebug("Linking to target lib %s\n", path.c_str());
		m_sources.emplace(path);
		m_dependencies.emplace(path);

		//Add a hint to the graph that we depend on it
		m_graph->AddTargetDependencyHint(f.GetArgs(), m_scriptpath);
	}

	//Add our link-time dependencies.
	//These are found by the OBJECT FILE dependency scan, since we need to know which libs exist at
	//source file scan time in order to set the right -DHAVE_xxx flags
	for(auto d : m_libdeps)
	{
		//LogDebug("[FPGABitstreamNode] Found library %s for %s\n", d.c_str(), GetFilePath().c_str());
		//and to our dependencies
		m_sources.emplace(d);
		m_dependencies.emplace(d);
	}
	for(auto f : m_libflags)
	{
		//LogDebug("[FPGABitstreamNode] Found library flag %s\n", static_cast<string>(f).c_str());
		m_flags.emplace(f);
	}

	//Finalize all of our dependencies
	for(auto d : m_dependencies)
	{
		//LogIndenter li;
		auto n = m_graph->GetNodeWithPath(d);
		if(n)
		{
			//LogDebug("Finalizing %s (%s)\n", d.c_str(), n->GetFilePath().c_str());
			n->Finalize();
		}
		else
			LogError("NULL node for path %s (in %s)\n", d.c_str(), GetFilePath().c_str());
	}

	UpdateHash();
	*/
}

FPGABitstreamNode::~FPGABitstreamNode()
{
}

/**
	@brief Calculate our hash. Must only be called from DoFinalize().
 */
void FPGABitstreamNode::UpdateHash()
{
	/*
	//Look up the working copy we're part of
	WorkingCopy* wc = m_graph->GetWorkingCopy();

	//Calculate our hash.
	//Dependencies and flags are obvious
	string hashin;
	for(auto d : m_dependencies)
		hashin += wc->GetFileHash(d);
	for(auto f : m_flags)
		hashin += sha256(f);

	//Need to hash both the toolchain AND the triplet since some toolchains can target multiple triplets
	hashin += g_nodeManager->GetToolchainHash(m_arch, m_toolchain);
	hashin += sha256(m_arch);

	//Do not hash the output file name.
	//Having multiple files with identical inputs merged into a single node is *desirable*.

	//Done, calculate final hash
	m_hash = sha256(hashin);
	*/
}