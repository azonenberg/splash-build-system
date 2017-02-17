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
	string board,
	BoardInfoFile* binfo,
	YAML::Node& node)
	: BuildGraphNode(graph, BuildFlag::FPGA_TIME, toolchain, arch, config, name, scriptpath, path, node)
	, m_board(board)
	, m_scriptpath(scriptpath)
	, m_netlist(NULL)
{
	//LogDebug("Creating FPGABitstreamNode (toolchain %s, output fname %s)\n",
	//	toolchain.c_str(), path.c_str());

	//If we specify a list of pins, read that
	map<string, int> pins;
	if(node["pins"])
	{
		auto npins = node["pins"];
		for(auto it : npins)
		{
			string pname = it.first.as<string>();
			pins[pname] = it.second.as<int>();
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
		GetDirOfFile(scriptpath) + "/" + name + ".foobar",
		board);
	if(!GenerateConstraintFile(pins, m_constrpath, binfo))
		return;

	//Look up all of the source files
	LoadSourceFileNodes(node, scriptpath, name, path, m_sourcenodes);

	//TODO: nocgen (and add to source nodes etc)

	//Create a flag for our speed grade
	char tmp[128];
	snprintf(tmp, sizeof(tmp), "hardware/speed/%d", binfo->GetSpeed());
	m_flags.emplace(BuildFlag(tmp));

	//Create a flag for our package
	snprintf(tmp, sizeof(tmp), "hardware/package/%s", binfo->GetPackage().c_str());
	m_flags.emplace(BuildFlag(tmp));
}

bool FPGABitstreamNode::GenerateConstraintFile(
		map<string, int>& pins,
		string path,
		BoardInfoFile* binfo)
{
	string constrs;

	//Get the constraint file extension and use different generators based on that
	if(path.find(".ucf") != string::npos)
	{
		if(!GenerateUCFConstraintFile(pins, path, binfo, constrs))
		{
			SetInvalidInput(string("ERROR: Constraint file generation failed: ") + constrs + "\n");
			return false;
		}
	}
	else if(path.find(".pcf") != string::npos)
	{
		if(!GeneratePCFConstraintFile(pins, path, binfo, constrs))
		{
			SetInvalidInput(string("ERROR: Constraint file generation failed: ") + constrs + "\n");
			return false;
		}
	}
	else
	{
		SetInvalidInput(string("ERROR: Don't know what to do with this constraint file: ") + path + ")\n");
		return false;
	}

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

	//Create a source file node for it, if needed
	if(!m_graph->HasNodeWithHash(hash))
		m_graph->AddNode(new SourceFileNode(m_graph, path, hash));

	return true;
}

/**
	@brief Generate a constraint file in Xilinx ISE UCF format
 */
bool FPGABitstreamNode::GenerateUCFConstraintFile(
		map<string, int>& pins,
		string /*path*/,
		BoardInfoFile* binfo,
		string& constraints)
{
	//LogDebug("Generating UCF file %s\n", path.c_str());

	//Header
	constraints = "";
	constraints += "# Automatically generated by Splash. DO NOT EDIT\n";
	constraints += "\n";

	//Create I/O constraints for each pin
	constraints += "########################################################################################\n";
	constraints += "# I/O CONSTRAINTS\n";
	constraints += "\n";
	for(auto it : pins)
	{
		auto name = it.first;
		auto w = it.second;

		for(int i=0; i<w; i++)
		{
			string sname = name;

			//If vector, format index etc
			if(w > 1)
			{
				char fullname[128];
				snprintf(fullname, sizeof(fullname), "%s[%d]", name.c_str(), i);
				sname = fullname;
			}

			if(!binfo->HasPin(sname))
			{
				constraints = "Board info has no pin named \"" + sname + "\"";
				return false;
			}
			auto bpin = binfo->GetPin(sname);

			auto netdec = string("NET \"") + sname + "\" ";
			constraints += netdec + "LOC = " + bpin.m_loc + ";\n";
			constraints += netdec + "IOSTANDARD = " + bpin.m_iostandard + ";\n";
			constraints += "\n";
		}
	}

	//Create timing constraints for clocks
	constraints += "########################################################################################\n";
	constraints += "# TIMING CONSTRAINTS\n";
	constraints += "\n";

	//For now, assume every top-level net on a clock location is actually being used as a clock
	for(auto it : pins)
	{
		auto name = it.first;

		//All clocks must be scalars (vector clock makes no sense)
		if(it.second != 1)
			continue;

		//If we don't have a matching clock, that's fine - just skip it
		if(!binfo->HasClock(name))
			continue;

		//Look up the clock speed/duty cycle
		auto bclk = binfo->GetClock(name);
		char tmp[128];
		snprintf(tmp, sizeof(tmp), "%lf MHz HIGH %d %%", bclk.m_speedMhz, (int)bclk.m_duty);

		//Render finished constraint
		constraints += string("NET \"") + name + "\" TNM_NET = \"" + name + "\";\n";
		constraints += string("TIMESPEC TS_") + name + " = PERIOD \"" + name + "\" " + tmp + ";\n";
	}

	return true;
}

/**
	@brief Generate a constraint file in openfpga PCF format.

	TODO: Disambiguate other things using the PCF extension (e.g. icestorm)?
 */
bool FPGABitstreamNode::GeneratePCFConstraintFile(
		map<string, int>& pins,
		string /*path*/,
		BoardInfoFile* binfo,
		string& constraints)
{
	//LogDebug("Generating PCF file %s\n", path.c_str());

	//Header
	constraints = "";
	constraints += "# Automatically generated by Splash. DO NOT EDIT\n";
	constraints += "\n";

	//Create I/O constraints for each pin
	constraints += "########################################################################################\n";
	constraints += "# I/O CONSTRAINTS\n";
	constraints += "\n";
	for(auto it : pins)
	{
		auto name = it.first;
		auto w = it.second;

		for(int i=0; i<w; i++)
		{
			string sname = name;

			//If vector, format index etc
			if(w > 1)
			{
				char fullname[128];
				snprintf(fullname, sizeof(fullname), "%s[%d]", name.c_str(), i);
				sname = fullname;
			}

			if(!binfo->HasPin(sname))
			{
				constraints = "Board info has no pin named \"" + sname + "\"";
				return false;
			}
			auto bpin = binfo->GetPin(sname);

			constraints += string("set_loc ") + sname + " " + bpin.m_loc + "\n";
			constraints += string("set_iostandard ") + sname + " " + bpin.m_iostandard + "\n";
		}

	}

	//Create timing constraints for clocks
	constraints += "########################################################################################\n";
	constraints += "# TIMING CONSTRAINTS\n";
	constraints += "\n";

	//For now, assume every top-level net on a clock location is actually being used as a clock
	for(auto it : pins)
	{
		auto name = it.first;

		//All clocks must be scalars (vector clock makes no sense)
		if(it.second != 1)
			continue;

		//If we don't have a matching clock, that's fine - just skip it
		if(!binfo->HasClock(name))
			continue;

		LogWarning("PCF timing constraints not implemented\n");

		/*
		//Look up the clock speed/duty cycle
		auto bclk = binfo->GetClock(name);
		char tmp[128];
		snprintf(tmp, sizeof(tmp), "%lf MHz HIGH %d %%", bclk.m_speedMhz, (int)bclk.m_duty);

		//Render finished constraint
		constraints += string("NET \"") + name + "\" TNM_NET = \"" + name + "\";\n";
		constraints += string("TIMESPEC TS_") + name + " = PERIOD \"" + name + "\" " + tmp + ";\n";
		*/
	}

	return true;
}


/**
	@brief Create all of our object files and kick off the dependency scans for them
 */
void FPGABitstreamNode::DoStartFinalization()
{
	//LogDebug("DoStartFinalization for %s\n", GetFilePath().c_str());
	//LogIndenter li;

	//Look up the working copy we're part of
	WorkingCopy* wc = m_graph->GetWorkingCopy();

	//Collect the flags for each step
	set<BuildFlag> synthFlags;
	GetFlagsForUseAt(BuildFlag::SYNTHESIS_TIME, synthFlags);

	//Map and PAR are sometimes the same step. Merge flags at this point of the process.
	set<BuildFlag> mapAndParFlags;
	GetFlagsForUseAt(BuildFlag::MAP_TIME, mapAndParFlags);
	GetFlagsForUseAt(BuildFlag::PAR_TIME, mapAndParFlags);

	//Bitstream generation is our job
	set<BuildFlag> imageFlags;
	GetFlagsForUseAt(BuildFlag::IMAGE_TIME, imageFlags);

	//We now have the constraints and the source files. Generate all of the intermediate stages.
	//TODO: Figure out how to adapt this for Vivado support

	//Make the synthesized output
	auto netpath = m_graph->GetIntermediateFilePath(
		m_toolchain,
		m_config,
		m_arch,
		"netlist",
		GetDirOfFile(m_scriptpath) + "/" + m_name + ".foobar",
		m_board);
	m_netlist = new HDLNetlistNode(
		m_graph,
		m_arch,
		m_config,
		m_name,
		m_scriptpath,
		netpath,
		m_toolchain,
		m_board,
		synthFlags,
		m_sourcenodes);

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
	set<string> ignored;
	wc->UpdateFile(netpath, h, false, false, ignored);

	//Look up a toolchain to query
	lock_guard<NodeManager> lock2(*g_nodeManager);
	Toolchain* chain = g_nodeManager->GetAnyToolchainForName(m_arch, m_toolchain);
	if(chain == NULL)
	{
		LogParseError("Could not find a toolchain of type %s targeting architecture arch %s\n",
			m_toolchain.c_str(), m_arch.c_str());
		return;
	}

	//If implementation and bitstream generation are separate steps, add a node for the physical netlist
	if(chain->IsTypeValid("circuit"))
	{
		//Make the physical netlist (translate and map if needed by our flow, then PAR)
		auto pnetpath = m_graph->GetIntermediateFilePath(
			m_toolchain,
			m_config,
			m_arch,
			"circuit",
			GetDirOfFile(m_scriptpath) + "/" + m_name + ".foobar",
			m_board);
		m_circuit = new PhysicalNetlistNode(
			m_graph,
			m_arch,
			m_config,
			m_name,
			m_scriptpath,
			pnetpath,
			m_toolchain,
			mapAndParFlags,
			m_netlist,
			m_constrpath);

		//If we have a node for this hash already, delete it and use the existing one. Otherwise use this
		h = m_circuit->GetHash();
		if(m_graph->HasNodeWithHash(h))
		{
			delete m_circuit;
			m_circuit = dynamic_cast<PhysicalNetlistNode*>(m_graph->GetNodeWithHash(h));
		}
		else
			m_graph->AddNode(m_circuit);
		m_dependencies.emplace(pnetpath);
		m_sources.emplace(pnetpath);
		wc->UpdateFile(pnetpath, h, false, false, ignored);
	}

	//Implementation and bitstream are one step, just use the netlist and constraint file as our input
	else
	{
		m_dependencies.emplace(netpath);
		m_sources.emplace(netpath);

		m_dependencies.emplace(m_constrpath);
		m_sources.emplace(m_constrpath);
	}
}

/**
	@brief Calculate our final hash etc
 */
void FPGABitstreamNode::DoFinalize()
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

FPGABitstreamNode::~FPGABitstreamNode()
{
}

/**
	@brief Calculate our hash. Must only be called from DoFinalize().
 */
void FPGABitstreamNode::UpdateHash()
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

	//Do not hash the output file name.
	//Having multiple files with identical inputs merged into a single node is *desirable*.

	//Done, calculate final hash
	m_hash = sha256(hashin);
}
