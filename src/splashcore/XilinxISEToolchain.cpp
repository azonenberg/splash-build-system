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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

XilinxISEToolchain::XilinxISEToolchain(string basepath, int major, int minor)
	: FPGAToolchain(basepath, TOOLCHAIN_ISE)
{
	//Save version info
	m_majorVersion = major;
	m_minorVersion = minor;
	m_patchVersion = 0;

	//Format the full version
	char tmp[128];
	snprintf(tmp, sizeof(tmp), "Xilinx ISE %d.%d", m_majorVersion, m_minorVersion);
	m_stringVersion = tmp;

	//Find toolchain binaries (TODO: non-Linux?)
	string arch = "lin";
	#if __x86_64__
		arch = "lin64";
	#endif
	m_binpath = basepath + "/ISE_DS/ISE/bin/" + arch;

	//Set the list of target architectures
	//For now, only WebPack devices.
	//TODO: determine if we have a full ISE license, add support for that
	//TODO: add more ISE device families

	m_triplets.emplace("coolrunner2-xc2c32a");
	m_triplets.emplace("coolrunner2-xc2c64a");
	m_triplets.emplace("coolrunner2-xc2c128");
	m_triplets.emplace("coolrunner2-xc2c256");

	m_triplets.emplace("spartan3a-xc3s50a");
	m_triplets.emplace("spartan3a-xc3s200a");

	m_triplets.emplace("spartan6-xc6slx4");
	m_triplets.emplace("spartan6-xc6slx9");
	m_triplets.emplace("spartan6-xc6slx16");
	m_triplets.emplace("spartan6-xc6slx25");
	m_triplets.emplace("spartan6-xc6slx25t");
	m_triplets.emplace("spartan6-xc6slx45");
	m_triplets.emplace("spartan6-xc6slx45t");
	m_triplets.emplace("spartan6-xc6slx75");
	m_triplets.emplace("spartan6-xc6slx75t");

	m_triplets.emplace("artix7-xc7a100t");
	m_triplets.emplace("artix7-xc7a200t");

	m_triplets.emplace("kintex7-xc7k70t");
	m_triplets.emplace("kintex7-xc7k160t");

	m_triplets.emplace("zynq7-xc7z010");
	m_triplets.emplace("zynq7-xc7z020");
	m_triplets.emplace("zynq7-xc7z030");

	//File format suffixes
	m_fixes["bitstream"] = stringpair("", ".bit");
	m_fixes["constraint"] = stringpair("", ".ucf");
	m_fixes["netlist"] = stringpair("", ".ngc");
	m_fixes["circuit"] = stringpair("", ".ncd");

	//Generate the hash
	m_hash = sha256(string("Xilinx ISE ") + m_stringVersion);
}

XilinxISEToolchain::~XilinxISEToolchain()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Toolchain properties

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual compilation

/**
	@brief Dispatch the build operation to the appropriate handlers
 */
bool XilinxISEToolchain::Build(
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	//TODO: Build simulations

	//If no sources, return fail b/c nothing to do
	if(sources.empty())
	{
		stdout = "ERROR: XilinxISEToolchain: No source files specified\n";
		return false;
	}

	//We have at least one source file, find the first one
	string src = *sources.begin();
	bool single_file = (sources.size() == 1);

	//If we have only one source and it's a .ngc we're map+PARing
	if(single_file && (src.find(".ngc") != string::npos) )
		return MapAndPar(triplet, sources, fname, flags, outputs, stdout);

	//If we have only one source and it's a .ncd we're bitgen'ing
	else if(single_file && (src.find(".ncd") != string::npos) )
		return GenerateBitstream(triplet, sources, fname, flags, outputs, stdout);

	//Assume synthesis by default
	else
		return Synthesize(triplet, sources, fname, flags, outputs, stdout);
}

/**
	@brief Synthesize a netlist
 */
bool XilinxISEToolchain::Synthesize(
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	stdout = "ERROR: XilinxISEToolchain::Synthesize() not implemented\n";

	/*
	//Print the heading
	DebugPrintfHeading(3, "Synthesizing NGC netlist file %s\n", output_filename.c_str());

	//Get output directory
	string path = GetDirOfFile(output_filename);
	string base = GetBasenameOfFileWithoutExt(output_filename);

	//Get the project root directory
	string project_root = CanonicalizePath(path + "/../..");

	//Write the .prj file
	string prj_file = path + "/" + base + ".prj";
	FILE* fp = fopen(prj_file.c_str(), "w");
	if(!fp)
		FatalError("Failed to create project file %s\n", prj_file.c_str());
	for(size_t i=0; i<source_files.size(); i++)
		fprintf(fp, "verilog work \"%s\"\n", source_files[i].c_str());
	fprintf(fp, "verilog work \"%s/ISE_DS/ISE/verilog/src/glbl.v\"\n", m_rootpath.c_str());
	fclose(fp);

	//Generate file names and make temporary directories
	string ngc_file = path + "/" + base + ".ngc";
	string xst_file = path + "/" + base + ".xst";
	string xst_dir = path + "/" + base + "_xst";
	string xst_tmpdir = path + "/projnav.tmp";
	if( (0 != mkdir(xst_dir.c_str(), 0755)) && (errno != EEXIST) )
		FatalError("Could not create XST directory %s\n", xst_dir.c_str());
	if( (0 != mkdir(xst_tmpdir.c_str(), 0755)) && (errno != EEXIST) )
		FatalError("Could not create XST temporary directory %s\n", xst_tmpdir.c_str());

	//Parse out the FPGA speed grade
	//Speed grade is the first digit after the first dash in the device name
	int speedgrade = 0;
	if(1 != sscanf(device.c_str(), "%*[^-]-%d", &speedgrade))
		FatalError("Unknown speed grade for device %s\n", device.c_str());

	//Create the XST input script
	fp = fopen(xst_file.c_str(), "w");
	if(!fp)
		FatalError("Failed to create XST script %s\n", xst_file.c_str());
	fprintf(fp, "set -tmpdir \"%s\"\n", xst_tmpdir.c_str());		//temporary directory
	fprintf(fp, "set -xsthdpdir \"%s\"\n", xst_dir.c_str());		//work directory
	fprintf(fp, "run\n");
	fprintf(fp, "-ifn %s\n", prj_file.c_str());						//list of sources
	fprintf(fp, "-ofn %s\n", ngc_file.c_str());						//netlist path
	fprintf(fp, "-ofmt NGC\n");										//Xilinx NGC netlist format
	fprintf(fp, "-p %s\n", device.c_str());							//part number
	fprintf(fp, "-top %s\n", top_level.c_str());					//top level module
	fprintf(fp, "-vlgincdir %s/generic/include\n", project_root.c_str());	//verilog include directory
	fprintf(fp, "-hierarchy_separator /\n");						//use / as hierarchy separator
	fprintf(fp, "-bus_delimiter []\n");								//use [] as bus delimeter
	fprintf(fp, "-case maintain\n");								//keep cases as is
	fprintf(fp, "-rtlview no\n");									//do not generate RTL schematic
	if(device.find("xc3s") == 0)
		fprintf(fp, "-define {XILINX_FPGA XILINX_SPARTAN3 XILINX_SPEEDGRADE=%d}\n", speedgrade);
	else if(device.find("xc6s") == 0)
		fprintf(fp, "-define {XILINX_FPGA XILINX_SPARTAN6 XILINX_SPEEDGRADE=%d}\n", speedgrade);
	else if(device.find("xc7a") == 0)
		fprintf(fp, "-define {XILINX_FPGA XILINX_7SERIES XILINX_ARTIX7 XILINX_SPEEDGRADE=%d}\n", speedgrade);
	else if(device.find("xc7k") == 0)
		fprintf(fp, "-define {XILINX_FPGA XILINX_7SERIES XILINX_KINTEX7 XILINX_SPEEDGRADE=%d}\n", speedgrade);
	else if(device.find("xc7v") == 0)
		fprintf(fp, "-define {XILINX_FPGA XILINX_7SERIES XILINX_VIRTEX7 XILINX_SPEEDGRADE=%d}\n", speedgrade);
	else if(device.find("xc2c") == 0)
		fprintf(fp, "-define {XILINX_CPLD XILINX_COOLRUNNER2 XILINX_SPEEDGRADE=%d}\n", speedgrade);

	if(device.find("xc2c") == string::npos)							//FPGA-specific flags
	{
		fprintf(fp, "-glob_opt AllClockNets\n");						//optimize clock periods
	}
	fprintf(fp, "-loop_iteration_limit 131072\n");					//loops can run up to 128K cycles
	for(size_t i=0; i<flags.size(); i++)
		fprintf(fp, "%s", flags[i]->toString(this).c_str());
	fclose(fp);

	//Create the run-xst script
	string xst_syr_file = path + "/" + base + ".syr";
	string xst_runscript = path + "/" + base + "_xst.sh";
	fp = fopen(xst_runscript.c_str(), "w");
	if(!fp)
		FatalError("Failed to create wrapper script %s\n", xst_runscript.c_str());
	fprintf(fp, "#!/bin/bash\n");
	fprintf(fp, "cd %s\n", path.c_str());
	fprintf(fp, "%s -intstyle xflow -ifn %s -ofn %s\n",
		m_xstpath.c_str(),
		xst_file.c_str(),
		xst_syr_file.c_str()
		);
	fprintf(fp, "if [ $? -ne 0 ]; then\n");
	fprintf(fp, "    touch build_failed\n");
	fprintf(fp, "    exit 1\n");
	fprintf(fp, "else\n");
	fprintf(fp, "    touch build_successful\n");
	fprintf(fp, "fi\n");
	fclose(fp);
	if(0 != chmod(xst_runscript.c_str(), 0755))
		FatalError("Failed to set execute permissions on XST run script %s\n", xst_runscript.c_str());

	//Submit the batch script
	return cluster->SubmitBatchJob(xst_runscript, cluster->m_fpgaBuildPartition, path, deps, 1, 1024);
	*/

	return false;
}

/**
	@brief Helper function to call Map() and Par()
 */
bool XilinxISEToolchain::MapAndPar(
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	if(!Map(triplet, sources, fname, flags, outputs, stdout))
		return false;
	return Par(triplet, sources, fname, flags, outputs, stdout);
}

/**
	@brief Technology mapping and initial placement
 */
bool XilinxISEToolchain::Map(
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	stdout = "ERROR: XilinxISEToolchain::Map() not implemented\n";
	return false;
}

/**
	@brief Routing
 */
bool XilinxISEToolchain::Par(
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	stdout = "ERROR: XilinxISEToolchain::Par() not implemented\n";
	return false;
}

/**
	@brief Bitstream generation
 */
bool XilinxISEToolchain::GenerateBitstream(
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	stdout = "ERROR: XilinxISEToolchain::GenerateBitstream() not implemented\n";
	return false;
}
