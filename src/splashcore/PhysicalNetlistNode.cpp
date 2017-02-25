/***********************************************************************************************************************
*                                                                                                                      *
* SPLASH build system v0.2                                                                                             *
*                                                                                                                      *
* Copyright (c) 2016-2017 Andrew D. Zonenberg                                                                          *
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

PhysicalNetlistNode::PhysicalNetlistNode(
	BuildGraph* graph,
	string arch,
	string /*config*/,
	string name,
	string scriptpath,
	string path,
	string toolchain,
	set<BuildFlag> flags,
	HDLNetlistNode* netlist,
	string constraints)
	: BuildGraphNode(graph, BuildFlag::COMPILE_TIME, toolchain, arch, name, scriptpath, path, flags)
	, m_constraintPath(constraints)
{
	//LogDebug("[%6.3f] Creating PhysicalNetlistNode %s\nfor arch %s, toolchain %s\n",
	//	g_scheduler->GetDT(), path.c_str(), arch.c_str(), toolchain.c_str() );
	//LogIndenter li;

	//Add an automatic dependency for the netlist file itself
	auto fname = netlist->GetFilePath();
	m_sources.emplace(fname);
	m_dependencies.emplace(fname);

	//We also need one for the constraint file
	m_sources.emplace(constraints);
	m_dependencies.emplace(constraints);

	//No dependency scanning needed here
}

PhysicalNetlistNode::~PhysicalNetlistNode()
{
}

void PhysicalNetlistNode::DoFinalize()
{
	auto wc = m_graph->GetWorkingCopy();

	//Calculate our hash.
	//Dependencies and flags are obvious
	//NOTE: This needs to happen *even if our dependency scan failed* so that we can identify the scan errors
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
}
