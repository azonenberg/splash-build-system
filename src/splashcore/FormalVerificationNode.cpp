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

FormalVerificationNode::FormalVerificationNode(
	BuildGraph* graph,
	string arch,
	string config,
	string name,
	string scriptpath,
	string path,
	string toolchain,
	YAML::Node& node)
	: BuildGraphNode(graph, BuildFlag::PROOF_TIME, toolchain, arch, config, name, scriptpath, path, node)
	, m_scriptpath(scriptpath)
{
	LogDebug("Creating FormalVerificationNode (toolchain %s, output fname %s)\n",
		toolchain.c_str(), path.c_str());
	LogIndenter li;

	//Look up all of the source files
	LoadSourceFileNodes(node, scriptpath, name, path, m_sourcenodes);
}

/**
	@brief Create all of our object files and kick off the dependency scans for them
 */
void FormalVerificationNode::DoStartFinalization()
{
	//LogDebug("DoStartFinalization for %s\n", GetFilePath().c_str());
	//LogIndenter li;

	//Look up the working copy we're part of
	WorkingCopy* wc = m_graph->GetWorkingCopy();

	//Collect the flags for each step
	set<BuildFlag> synthFlags;
	GetFlagsForUseAt(BuildFlag::SYNTHESIS_TIME, synthFlags);

	//Make the synthesized output
	auto netpath = m_graph->GetIntermediateFilePath(
		m_toolchain,
		m_config,
		m_arch,
		"formal-netlist",
		GetDirOfFile(m_scriptpath) + "/" + m_name + ".foobar");
	m_netlist = new HDLNetlistNode(
		m_graph,
		m_arch,
		m_config,
		m_name,
		m_scriptpath,
		netpath,
		m_toolchain,
		"formal",
		synthFlags,
		m_sourcenodes);

	//Add constraints file (if we have one)
	for(auto f : m_sourcenodes)
	{
		string path = f->GetFilePath();
		if(path.find(".smtc") != string::npos)
			m_sources.emplace(path);
	}

	//If we have a node for this hash already, delete it and use the existing one. Otherwise use this
	string h = m_netlist->GetHash();
	if(m_graph->HasNodeWithHash(h))
	{
		delete m_netlist;
		m_netlist = dynamic_cast<HDLNetlistNode*>(m_graph->GetNodeWithHash(h));
	}
	else
		m_graph->AddNode(m_netlist);
	m_dependencies.emplace(netpath);
	m_sources.emplace(netpath);
	set<string> ignored;
	wc->UpdateFile(netpath, h, false, false, ignored);
}

/**
	@brief Calculate our final hash etc
 */
void FormalVerificationNode::DoFinalize()
{
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
}

FormalVerificationNode::~FormalVerificationNode()
{
}

/**
	@brief Calculate our hash. Must only be called from DoFinalize().
 */
void FormalVerificationNode::UpdateHash()
{
	//TODO: more advanced stuff related to our specific setup

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

	//Done, calculate final hash
	m_hash = sha256(hashin);
}
