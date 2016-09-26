/***********************************************************************************************************************
*                                                                                                                      *
* SPLASH build system v0.2                                                                                             *
*                                                                                                                      *
* Copyright (c) 2016 Andrew D. Zonenberg                                                                               *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in Object and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of Object code must retain the above copyright notice, this list of conditions, and the         *
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

CPPObjectNode::CPPObjectNode(
	BuildGraph* graph,
	string arch,
	string fname,
	string path,
	string toolchain,
	string script,
	set<BuildFlag> flags)
	: BuildGraphNode(graph, toolchain, arch, fname, path, flags)
{
	m_script = script;

	LogDebug("        Creating CPPObjectNode %s (from source file %s) for arch %s, toolchain %s\n",
		path.c_str(), fname.c_str(), arch.c_str(), toolchain.c_str() );

	//Run the dependency scanner on this file to see what other stuff we need to pull in.
	//This will likely require pulling a lot of files from the golden node.
	//TODO: handle generated headers, etc
	//If the scan fails, give up and declare us un-buildable
	set<string> deps;
	if(!g_scheduler->ScanDependencies(fname, arch, toolchain, flags, graph->GetWorkingCopy(), deps))
	{
		m_invalidInput = true;
		return;
	}

	//Add a dependency for the source file itself
	auto wc = graph->GetWorkingCopy();
	auto h = wc->GetFileHash(fname);
	if(!graph->HasNodeWithHash(h))
		graph->AddNode(new CPPSourceNode(graph, fname, h));
	m_sources.emplace(fname);
	m_dependencies.emplace(fname);

	//Add source nodes if we don't have them already
	for(auto d : deps)
	{
		auto h = wc->GetFileHash(d);

		//Create a new node if needed
		if(!graph->HasNodeWithHash(h))
			graph->AddNode(new CPPSourceNode(graph, d, h));

		//Either way, we have the node now. Add it to our list of inputs.
		m_dependencies.emplace(d);
	}

	//Dump the output
	/*
	for(auto d : m_dependencies)
	{
		auto h = wc->GetFileHash(d);
		LogDebug("            dependency %s (%s)\n",
			d.c_str(),
			h.c_str());
	}
	*/

	//Calculate our hash.
	//Dependencies and flags are obvious
	string hashin;
	for(auto d : m_dependencies)
		hashin += wc->GetFileHash(d);
	for(auto f : flags)
		hashin += sha256(f);

	//Need to hash both the toolchain AND the triplet since some toolchains can target multiple triplets
	hashin += g_nodeManager->GetToolchainHash(arch, toolchain);
	hashin += sha256(arch);

	//Do not hash the output file name.
	//Having multiple files with identical inputs merged into a single node is *desirable*.

	//Done, calculate final hash
	m_hash = sha256(hashin);
}

CPPObjectNode::~CPPObjectNode()
{
}

