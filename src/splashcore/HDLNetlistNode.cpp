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

HDLNetlistNode::HDLNetlistNode(
	BuildGraph* graph,
	string arch,
	string /*config*/,
	string name,
	string scriptpath,
	string path,
	string toolchain,
	string board,
	string binfohash,
	set<BuildFlag> flags,
	set<BuildGraphNode*> sources)
	: BuildGraphNode(graph, BuildFlag::SYNTHESIS_TIME, toolchain, arch, name, scriptpath, path, flags)
	, m_board(board)
	, m_binfohash(binfohash)
{
	/*
	LogDebug("[%6.3f] Creating HDLNetlistNode %s\nfor arch %s, toolchain %s, board %s\n",
		g_scheduler->GetDT(), path.c_str(), arch.c_str(), toolchain.c_str(),
		GetBasenameOfFileWithoutExt(board).c_str() );
	LogIndenter li;
	*/

	//Include files in our working directory automatically
	string incdir = GetDirOfFile(scriptpath);
	m_flags.emplace(BuildFlag(string("library/__incdir/") + incdir));

	//Add an automatic dependency for the source files
	for(auto src : sources)
	{
		auto f = src->GetFilePath();
		m_dependencies.emplace(f);
		m_sources.emplace(f);
	}
}

HDLNetlistNode::~HDLNetlistNode()
{
}

bool HDLNetlistNode::ScanDependencies(string fname)
{
	//Read the source code for this file
	auto wc = m_graph->GetWorkingCopy();
	if(!wc->HasFile(fname))
	{
		SetInvalidInput(string("ERROR: Couldn't find source file ") + fname + " for dependency scan\n");
		return false;
	}
	string hash = wc->GetFileHash(fname);
	string source = g_cache->ReadCachedFile(hash);

	//Split the source file by lines
	vector<string> lines;
	ParseLines(source, lines);

	//Do very minimal parsing of the lines to look for dependencies
	for(auto line : lines)
	{
		//Eat spaces
		while(!line.empty() && isspace(line[0]))
			line = line.substr(1);

		//TODO: Parse multiline comments

		string cpath;

		//Look for preprocessor commands
		if(line[0] == '`')
		{
			//TODO: Implement `if etc
			//For now, skip anything but include statements
			if(line.find("`include") != 0)
				continue;

			//Look up the filename
			size_t start = line.find("\"");
			size_t end = line.find("\"", start+1);
			if( (start == string::npos) || (end == string::npos) )
			{
				SetInvalidInput("ERROR: Malformed `include line\n");
				return false;
			}
			string fpath = line.substr(start+1, end-(start+1));

			//Canonicalize based on our working directory and the original directory
			string cpath_guess1 = GetDirOfFile(m_path) + "/" + fpath;
			string cpath_guess2 = GetDirOfFile(fname) + "/" + fpath;
			string cpath_guess3 = GetDirOfFile(m_script) + "/" + fpath;

			if(CanonicalizePathThatMightNotExist(cpath_guess1) && cpath_guess1 != "" && wc->HasFile(cpath_guess1))
			{
				cpath = cpath_guess1;
				//LogDebug("Resolved include statement via project dir: %s (%s)\n", fpath.c_str(), cpath.c_str());
			}

			else if(CanonicalizePathThatMightNotExist(cpath_guess2) && cpath_guess2 != "" && wc->HasFile(cpath_guess2))
			{
				cpath = cpath_guess2;
				//LogDebug("Resolved include statement via source file dir: %s (%s)\n", fpath.c_str(), cpath.c_str());
			}

			else if(CanonicalizePathThatMightNotExist(cpath_guess3) && cpath_guess3 != "" && wc->HasFile(cpath_guess3))
			{
				cpath = cpath_guess3;
				//LogDebug("Resolved include statement via build script dir: %s (%s)\n", fpath.c_str(), cpath.c_str());
			}

			else
			{
				LogWarning(
					"Failed to resolve include path: %s\n"
					"    cpath_guess1 = %s\n"
					"    cpath_guess2 = %s\n"
					"    cpath_guess3 = %s\n",
					fpath.c_str(),
					cpath_guess1.c_str(),
					cpath_guess2.c_str(),
					cpath_guess3.c_str());
			}
		}

		//Look for system tasks
		//FIXME: Doesn't process full HDL elaboration, will fail horribly if the filename is passed as a
		//parameter to the module!
		if(line[0] == '$')
		{
			//For now, skip anything but $readmem
			if(line.find("$readmem") != 0)
				continue;

			//Look up the filename
			size_t start = line.find("\"");
			size_t end = line.find("\"", start+1);
			if( (start == string::npos) || (end == string::npos) )
			{
				LogWarning("Couldn't parse $readmem argument (probably not hard coded): %s\n", line.c_str());
				continue;
			}
			string fpath = line.substr(start+1, end-(start+1));

			//Canonicalize based on our working directory
			cpath = GetDirOfFile(fname) + "/" + fpath;
			if(!CanonicalizePathThatMightNotExist(cpath))
			{
				SetInvalidInput(string("ERROR: Couldn't canonicalize $readmem file ") + cpath + "\n");
				return false;
			}

			//LogDebug("Found readmem: %s (%s)\n", fpath.c_str(), cpath.c_str());
		}

		//Skip if we didn't find anything
		if(cpath.empty())
			continue;

		//See if it exists
		if(!wc->HasFile(cpath))
		{
			SetInvalidInput(string("ERROR: Include file ") + cpath + " doesn't exist\n");
			return false;
		}

		//Add a source file node for it (if we don't have one already)
		auto h = wc->GetFileHash(cpath);
		if(!m_graph->HasNodeWithHash(h))
			m_graph->AddNode(new SourceFileNode(m_graph, cpath, h));

		//Pull it in
		m_dependencies.emplace(cpath);
	}

	return true;
}

void HDLNetlistNode::DoFinalize()
{
	auto wc = m_graph->GetWorkingCopy();

	//Run the dependency scanner once all nodes have been created
	for(auto f : m_sources)
	{
		//Run the dependency scanner on this file to see what other stuff we need to pull in.
		//For now, do not support recursive includes (i.e. we only scan this source file).
		//If the scanner fails, immediately return and stay failed with our temporary hash
		if(!ScanDependencies(f))
			return;
	}

	//Calculate our hash.
	//Dependencies and flags are obvious
	//NOTE: This needs to happen *even if our dependency scan failed* so that we can identify the scan errors
	string hashin;
	for(auto d : m_dependencies)
		hashin += wc->GetFileHash(d);
	for(auto f : m_flags)
		hashin += sha256(f);

	//Need to hash both the toolchain AND the triplet since some toolchains can target multiple triplets
	//Also hash board since pinout etc depend on that
	//BUGFIX: hash contents of board info file, not just the name!
	hashin += g_nodeManager->GetToolchainHash(m_arch, m_toolchain);
	hashin += sha256(m_arch);
	hashin += sha256(m_binfohash);

	//Do not hash the output file name.
	//Having multiple files with identical inputs merged into a single node is *desirable*.

	//Done, calculate final hash
	m_hash = sha256(hashin);
}
