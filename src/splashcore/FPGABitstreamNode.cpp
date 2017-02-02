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
	BoardInfoFile* binfo,
	YAML::Node& node)
	: BuildGraphNode(graph, BuildFlag::LINK_TIME, toolchain, arch, config, name, scriptpath, path, node)
	, m_scriptpath(scriptpath)
{
	LogDebug("Creating FPGABitstreamNode (toolchain %s, output fname %s)\n",
		toolchain.c_str(), path.c_str());

	//If we specify a list of pins, read that
	map<string, TopLevelPin> pins;
	if(node["pins"])
	{
		auto npins = node["pins"];
		for(auto it : npins)
		{
			string pname = it.first.as<string>();
			pins[pname] = TopLevelPin(it.second);
		}
	}
	else
	{
		LogError("no pins\n");
		SetInvalidInput("No top-level pins specified (this bitstream is useless!)\n");
		return;
	}

	//Generate our constraint file
	m_constrpath = m_graph->GetIntermediateFilePath(
		toolchain,
		config,
		arch,
		"constraint",
		GetDirOfFile(scriptpath) + "/" + name + ".foobar");
	GenerateConstraintFile(pins, m_constrpath, binfo);

	//Look up all of the source files
	LoadSourceFileNodes(node, scriptpath, name, path, m_sourcenodes);

	//TODO: nocgen (and add to source nodes etc)
}

void FPGABitstreamNode::GenerateConstraintFile(
		map<string, TopLevelPin>& pins,
		string path,
		BoardInfoFile* binfo)
{
	string constrs;

	//Get the constraint file extension and use different generators based on that
	if(path.find(".ucf") != string::npos)
		constrs = GenerateUCFConstraintFile(pins, path, binfo);
	else
		LogError("FPGABitstream: Don't know what to do with this constraint file extension\n");

	//DEBUG: Print constraint file
	//LogDebug("GENERATED CONSTRAINTS:\n");
	//LogIndenter li;
	//LogDebug("%s\n", constrs.c_str());

	//Now that we have the constraints, put them in the working copy
	//Don't worry about dirty scripts
	string hash = sha256(constrs);
	g_cache->AddFile(GetBasenameOfFile(path), hash, hash, constrs, "");
	set<string> ignored;
	m_graph->GetWorkingCopy()->UpdateFile(path, hash, false, false, ignored);
}

string FPGABitstreamNode::GenerateUCFConstraintFile(
		map<string, TopLevelPin>& pins,
		string path,
		BoardInfoFile* binfo)
{
	LogDebug("Generating UCF file %s\n", path.c_str());

	//Header
	string retval;
	retval += "# Automatically generated by Splash. DO NOT EDIT\n";
	retval += "\n";

	//Create I/O constraints for each pin
	retval += "########################################################################################\n";
	retval += "# I/O CONSTRAINTS\n";
	retval += "\n";
	for(auto it : pins)
	{
		auto name = it.first;
		auto tlp = it.second;

		//Scalar pins are easy: just emit the pin itself
		if(tlp.GetWidth() == 1)
		{
			if(!binfo->HasPin(name))
			{
				LogError("Board info has no pin named \"%s\"\n", name.c_str());
				return "";
			}
			auto bpin = binfo->GetPin(name);

			auto netdec = string("NET \"") + name + "\" ";
			retval += netdec + "LOC = " + bpin.m_loc + ";\n";
			retval += netdec + "IOSTANDARD = " + bpin.m_iostandard + ";\n";
			retval += "\n";
		}

		//Vectors need a constraint for each bit
		else
		{
			LogError("Vector pin constraints not yet implemented\n");
			return "";
		}
	}

	//Create timing constraints for clocks
	retval += "########################################################################################\n";
	retval += "# TIMING CONSTRAINTS\n";
	retval += "\n";

	//For now, assume every top-level net on a clock location is actually being used as a clock
	for(auto it : pins)
	{
		auto name = it.first;
		auto tlp = it.second;

		//All clocks must be scalars (vector clock makes no sense)
		if(tlp.GetWidth() != 1)
			continue;

		//If we don't have a matching clock, that's fine - just skip it
		if(!binfo->HasClock(name))
			continue;

		//Look up the clock speed/duty cycle
		auto bclk = binfo->GetClock(name);
		char tmp[128];
		snprintf(tmp, sizeof(tmp), "%lf MHz HIGH %d %%", bclk.m_speedMhz, (int)bclk.m_duty);

		//Render finished constraint
		retval += string("NET \"") + name + "\" TNM_NET = \"" + name + "\";\n";
		retval += string("TIMESPEC TS_") + name + " = PERIOD \"" + name + "\" " + tmp + ";\n";
	}

	return retval;
}

/**
	@brief Create all of our object files and kick off the dependency scans for them
 */
void FPGABitstreamNode::DoStartFinalization()
{
	LogDebug("DoStartFinalization for %s\n", GetFilePath().c_str());
	LogIndenter li;

	//Look up the working copy we're part of
	WorkingCopy* wc = m_graph->GetWorkingCopy();

	//Collect the flags for each step
	set<BuildFlag> synthFlags;
	GetFlagsForUseAt(BuildFlag::SYNTHESIS_TIME, synthFlags);

	set<BuildFlag> mapFlags;
	GetFlagsForUseAt(BuildFlag::MAP_TIME, mapFlags);

	set<BuildFlag> parFlags;
	GetFlagsForUseAt(BuildFlag::PAR_TIME, parFlags);

	set<BuildFlag> imageFlags;
	GetFlagsForUseAt(BuildFlag::IMAGE_TIME, imageFlags);

	//We now have the constraints and the source files. Generate all of the intermediate stages.
	//TODO: Figure out how to adapt this for Vivado support

	//First pass: Make the HDLNetlistNode
	LogError("FIXME: FPGABitstreamNode doesn't know how to make synthesis results node yet\n");

	/*

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
	BuildGraphNode::UpdateHash_DefaultTarget();
}
