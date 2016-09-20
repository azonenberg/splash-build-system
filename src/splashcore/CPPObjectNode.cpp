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
	set<BuildFlag> flags)
{
	LogDebug("        Creating CPPObjectNode %s (from source file %s) for arch %s, toolchain %s\n",
		path.c_str(), fname.c_str(), arch.c_str(), toolchain.c_str() );


	//Run the dependency scanner on this file to see what other stuff we need to pull in.
	//This will likely require pulling a lot of files from the golden node.
	//TODO: handle generated headers, etc
	set<string> deps;
	g_scheduler->ScanDependencies(fname, arch, toolchain, flags, graph->GetWorkingCopy(), deps);

	//Add source nodes if we don't have them already
	auto wc = graph->GetWorkingCopy();
	for(auto d : deps)
	{
		string h = wc->GetFileHash(d);

		//Already there
		if(graph->HasNodeWithHash(h))
			LogDebug("            File %s has hash %s, already in graph\n", d.c_str(), h.c_str());

		//Create new node
		else
		{
			LogDebug("            File %s has hash %s, adding to graph\n", d.c_str(), h.c_str());
			graph->AddNode(new CPPSourceNode(graph, d, h));
		}

		//TODO: Add to our list of inputs
	}

	/*
	//Dump the output
	LogDebug("    CPPObjectNode: dep scan done\n");
	for(auto d : deps)
		LogDebug("        %s\n", d.c_str());
	*/
}

CPPObjectNode::~CPPObjectNode()
{
}

