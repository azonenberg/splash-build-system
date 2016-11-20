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

ConstantTableNode::ConstantTableNode(
	BuildGraph* graph,
	string fname,
	string tablefname,
	string generator,
	string scriptpath)
	: BuildGraphNode(graph, BuildFlag::NO_TIME, fname, "")
{
	//TODO
	LogDebug("Creating constant table %s (from table %s, generator %s, script %s)\n",
		fname.c_str(),
		tablefname.c_str(),
		generator.c_str(),
		scriptpath.c_str());
}

void ConstantTableNode::DoFinalize()
{
	//nothing to do, all the work is handled in the ctor
}

ConstantTableNode::~ConstantTableNode()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Table generation

string ConstantTableNode::GetOutputBasename(string basename, string generator)
{
	//string dir = GetDirOfFile(scriptpath);

	if(generator == "c/enum")
		return basename + "_enum.h";
	else if(generator == "c/define")
		return basename + "_define.h";
	else if(generator == "verilog/define")
		return basename + "_define.vh";
	else if(generator == "verilog/localparam")
		return basename + "_localparam.vh";
	else
	{
		LogError("Invalid generator %s\n", generator.c_str());
		return "";
	}
}
