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

CPPExecutableNode::CPPExecutableNode(
	BuildGraph* graph,
	string arch,
	string config,
	string name,
	string scriptpath,
	string path,
	string toolchain,
	YAML::Node& node)
	: BuildGraphNode(graph, toolchain, arch, config, name, scriptpath, path, node)
{
	LogDebug("    Creating CPPExecutableNode (toolchain %s, output fname %s)\n",
		toolchain.c_str(), path.c_str());

	//Sanity check: we must have some source files!
	if(!node["sources"])
	{
		LogParseError(
			"CPPExecutableNode: cannot have a C++ executable (%s, declared in %s) without any source files\n",
			name.c_str(),
			path.c_str()
			);
		return;
	}
	auto snode = node["sources"];

	//Look up the working copy we're part of
	WorkingCopy* wc = m_graph->GetWorkingCopy();

	//Collect the compiler flags
	unordered_set<BuildFlag> compileFlags;
	GetFlagsForUseAt(BuildFlag::COMPILE_TIME, compileFlags);
	for(auto x : compileFlags)
		LogDebug("        Compile flag: %s\n", static_cast<string>(x).c_str());

	//Create source nodes, if they don't already exist

	/*
	//Read the sources section and create an object node for each one
	for(auto it : snode)
	{
		//File name is relative to the build script.
		//Get the actual path name (TODO: canonicalize ../ etc)
		string fname = (GetDirOfFile(path) + "/" + it.as<std::string>());

		//TODO
	}
	*/

	//Generate our hash
	//FIXME: just use our pointer
	char tmp[32];
	snprintf(tmp, sizeof(tmp), "%p", this);
	m_hash = sha256(tmp);
}

CPPExecutableNode::~CPPExecutableNode()
{
}

