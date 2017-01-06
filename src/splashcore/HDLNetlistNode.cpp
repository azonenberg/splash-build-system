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
	string fname,
	string path,
	string toolchain,
	string script,
	set<BuildFlag> flags/*,
	set<string>& libdeps,
	set<BuildFlag>& libflags*/)
	: BuildGraphNode(graph, BuildFlag::COMPILE_TIME, toolchain, arch, fname, path, flags)
{
	/*
	m_script = script;

	//LogDebug("[%6.3f] Creating HDLNetlistNode %s (from src %s)\nfor arch %s, toolchain %s\n",
	//	g_scheduler->GetDT(), path.c_str(), fname.c_str(), arch.c_str(), toolchain.c_str() );
	//LogIndenter li;

	//Add an automatic dependency for the source file itself
	auto wc = graph->GetWorkingCopy();
	auto h = wc->GetFileHash(fname);
	if(!graph->HasNodeWithHash(h))
		graph->AddNode(new SourceFileNode(graph, fname, h));
	m_sources.emplace(fname);
	m_dependencies.emplace(fname);

	//Run the dependency scanner on this file to see what other stuff we need to pull in.
	//This will likely require pulling a lot of files from the golden node.
	//If the scan fails, declare us un-buildable
	m_scanJob = g_scheduler->ScanDependenciesNonblocking(
		fname,
		arch,
		toolchain,
		flags,
		graph->GetWorkingCopy());
	if(m_scanJob == NULL)
	{
		SetInvalidInput("Failed to start dependency scanner\n");
	}
	*/
}

HDLNetlistNode::~HDLNetlistNode()
{
}

/**
	@brief Get the results of our library search
 */
void HDLNetlistNode::GetLibraryScanResults(
	set<string>& libdeps,
	set<BuildFlag>& libflags)
{
	/*
	Finalize();

	for(auto d : m_libdeps)
		libdeps.emplace(d);
	for(auto f : m_libflags)
		libflags.emplace(f);
	*/
}

void HDLNetlistNode::DoFinalize()
{
	/*
	auto wc = m_graph->GetWorkingCopy();

	//Block until the scan completes
	set<string> deps;
	set<BuildFlag> foundflags;
	if(g_scheduler->BlockOnScanResults(m_scanJob, wc, deps, foundflags, m_errors))
	{
		//Dependencies scanned OK, update our stuff
		//Update our flags with the HAVE_xx macros from the libraries we located
		for(auto f : foundflags)
			m_flags.emplace(f);

		//Get the library extensions
		string libpre;
		string shlibsuf;
		string statsuf;
		string osuf;
		{
			lock_guard<NodeManager> lock(*g_nodeManager);
			auto chain = g_nodeManager->GetAnyToolchainForName(m_arch, m_toolchain);
			libpre = chain->GetPrefix("shlib");
			shlibsuf = chain->GetSuffix("shlib");
			statsuf = chain->GetSuffix("stlib");
			osuf = chain->GetSuffix("object");
		}

		//Go over the list of libraries we asked for and remove any optional ones we don't have
		//TODO: export library set to caller for link stage?
		set<BuildFlag> flagsToErase;
		for(auto f : m_flags)
		{
			if(f.GetType() != BuildFlag::TYPE_LIBRARY)
				continue;

			//Search for any file with a basename containing the lib name
			bool found = false;
			string base = libpre + f.GetArgs();
			string libpath;
			for(auto d : deps)
			{
				auto search = GetBasenameOfFile(d);
				if(search.find(base) != 0)
					continue;
				if( (search.find(shlibsuf) != string::npos) || (search.find(statsuf) != string::npos) )
				{
					libpath = d;
					//LogDebug("%s is a hit for %s\n", d.c_str(), static_cast<string>(f).c_str());
					found = true;
					break;
				}
			}

			//If we did NOT find the library, remove from the list of libraries to link
			if(!found)
			{
				//LogDebug("Did not find a hit for %s, removing\n", static_cast<string>(f).c_str());
				flagsToErase.emplace(f);
			}

			//If we DID find it, add it as a dependency for the executable
			else
			{
				m_libdeps.emplace(libpath);
				m_libflags.emplace(f);

				//REMOVE the library from OUR dependencies though, no point in pulling it down when compiling
				deps.erase(libpath);

				//If it wasn't already in the graph, create a node for it
				string hash = wc->GetFileHash(libpath);
				if(!m_graph->HasNodeWithHash(hash))
				{
					//LogDebug("[1] Creating SystemLibraryNode %s\n  with hash %s, graph %p\n",
					//	libpath.c_str(), hash.c_str(), graph);
					m_graph->AddNode(new SystemLibraryNode(m_graph, libpath, hash));
				}
			}
		}

		//can't do this in the loop above or iterators go bad
		for(auto f : flagsToErase)
			m_flags.erase(f);

		//Make a note of any object or library files we have in the dependency list that were not specified by flags
		for(auto d : deps)
		{
			if( (d.find(osuf) == string::npos) &&
				(d.find(shlibsuf) == string::npos) &&
				(d.find(statsuf) == string::npos) )
			{
				continue;
			}

			m_libdeps.emplace(d);
			//LogDebug("found object/library dep %s\n", d.c_str());

			//If it wasn't already in the graph, create a node for it
			string hash = wc->GetFileHash(d);
			if(!m_graph->HasNodeWithHash(hash))
			{
				//LogDebug("[2] Creating SystemLibraryNode %s\n  with hash %s, graph %p\n",
				//	d.c_str(), hash.c_str(), graph);
				m_graph->AddNode(new SystemLibraryNode(m_graph, d, hash));
			}
		}

		//Add source nodes if we don't have them already
		for(auto d : deps)
		{
			auto h = wc->GetFileHash(d);

			//See what type of file we got as a dependency.

			//Create a new node if needed
			if(!m_graph->HasNodeWithHash(h))
				m_graph->AddNode(new SourceFileNode(m_graph, d, h));

			//Either way, we have the node now. Add it to our list of inputs.
			m_dependencies.emplace(d);
			//LogDebug("Adding %s as dependency to %s\n", d.c_str(), GetFilePath().c_str());
		}


		//Dump the output
		//for(auto d : m_dependencies)
		//{
		//	auto h = wc->GetFileHash(d);
		//	LogDebug("dependency %s (%s)\n",
		//		d.c_str(),
		//		h.c_str());
		//}
	}

	//DEBUG: Dump flag
	//for(auto f : m_flags)
	//	LogDebug("Flag: %s\n", static_cast<string>(f).c_str());

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

	//If the dependency scan failed, add a dummy cached file with the proper ID and stdout
	//so we can query the result in the cache later on.
	if(m_errors != "")
		SetInvalidInput(m_errors);
	*/
}
