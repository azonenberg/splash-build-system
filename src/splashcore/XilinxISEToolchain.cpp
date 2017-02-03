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
	stdout = "";

	LogDebug("XilinxISEToolchain::Synthesize for %s\n", fname.c_str());
	string base = GetBasenameOfFileWithoutExt(fname);

	//Write the .prj file
	string prj_file = base + ".prj";
	FILE* fp = fopen(prj_file.c_str(), "w");
	if(!fp)
	{
		stdout += string("ERROR: Failed to create project file ") + prj_file + "\n";
		return false;
	}
	for(auto s : sources)
		fprintf(fp, "verilog work \"%s\"\n", s.c_str());
	fprintf(fp, "verilog work \"%s/ISE_DS/ISE/verilog/src/glbl.v\"\n", m_basepath.c_str());
	fclose(fp);

	//Generate file names and make temporary directories
	string ngc_file = base + ".ngc";
	string xst_file = base + ".xst";
	string report_file = base + ".syr";
	string xst_dir = base + "_xst";
	string xst_tmpdir = "xst_tmp";
	if( (0 != mkdir(xst_dir.c_str(), 0755)) && (errno != EEXIST) )
	{
		stdout += string("ERROR: Failed to create XST directory ") + xst_dir + "\n";
		return false;
	}
	if( (0 != mkdir(xst_tmpdir.c_str(), 0755)) && (errno != EEXIST) )
	{
		stdout += string("ERROR: Failed to create XST directory ") + xst_tmpdir + "\n";
		return false;
	}

	//Go over our flags and figure out our speed grade and package
	int speed = 0;
	string package;
	for(auto f : flags)
	{
		if(f.GetType() != BuildFlag::TYPE_HARDWARE)
			continue;

		if(f.GetFlag() == "speed")
			speed = atoi(f.GetArgs().c_str());
		else if(f.GetFlag() == "package")
			package = f.GetArgs();
	}
	if(speed <= 0)
	{
		stdout += string("ERROR: Device speed grade (for ") + fname + ") not specified\n";
		return false;
	}
	if(package == "")
	{
		stdout += string("ERROR: Device package (for ") + fname + ") not specified\n";
		return false;
	}

	//Format the part name
	string device = triplet.substr(triplet.find("-") + 1);
	char sdev[128];
	snprintf(sdev, sizeof(sdev), "%s-%d%s",
		device.c_str(),
		speed,
		package.c_str());
	device = sdev;
	LogDebug("Final device name: %s\n", device.c_str());

	//Create the XST input script
	//For now, top level module name must equal file base name
	fp = fopen(xst_file.c_str(), "w");
	if(!fp)
	{
		stdout += string("ERROR: Failed to create XST script ") + xst_file + "\n";
		return false;
	}
	fprintf(fp, "set -tmpdir \"%s\"\n", xst_tmpdir.c_str());		//temporary directory
	fprintf(fp, "set -xsthdpdir \"%s\"\n", xst_dir.c_str());		//work directory
	fprintf(fp, "run\n");
	fprintf(fp, "-ifn %s\n", prj_file.c_str());						//list of sources
	fprintf(fp, "-ofn %s\n", fname.c_str());						//netlist path
	fprintf(fp, "-ofmt NGC\n");										//Xilinx NGC netlist format
	fprintf(fp, "-p %s\n", device.c_str());							//part number
	fprintf(fp, "-top %s\n", base.c_str());							//top level module (file basename)
	//fprintf(fp, "-vlgincdir %s/generic/include\n", project_root.c_str());	//verilog include directory
																			//TODO: implement this
	fprintf(fp, "-hierarchy_separator /\n");						//use / as hierarchy separator
	fprintf(fp, "-bus_delimiter []\n");								//use [] as bus delimeter
	fprintf(fp, "-case maintain\n");								//keep cases as is
	fprintf(fp, "-rtlview no\n");									//do not generate RTL schematic
	if(device.find("coolrunner2-") == string::npos)					//FPGA-specific flags
		fprintf(fp, "-glob_opt AllClockNets\n");					//optimize clock periods
	fprintf(fp, "-loop_iteration_limit 131072\n");					//loops can run up to 128K cycles
																	//TODO: figure out details?

	//Add some `define flags of our own
	string defines;
	if(device.find("spartan3-") == 0)
		defines += "XILINX_FPGA XILINX_SPARTAN3 ";
	else if(device.find("spartan6-") == 0)
		defines += "XILINX_FPGA XILINX_SPARTAN6 ";
	else if(device.find("artix7-") == 0)
		defines += "XILINX_FPGA XILINX_7SERIES XILINX_ARTIX7 ";
	else if(device.find("kintex7-") == 0)
		defines += "XILINX_FPGA XILINX_7SERIES XILINX_KINTEX7 ";
	else if(device.find("virtex7-") == 0)
		defines += "XILINX_FPGA XILINX_7SERIES XILINX_VIRTEX7 ";
	else if(device.find("coolrunner2-") == 0)
		defines += "XILINX_CPLD XILINX_COOLRUNNER2 ";
	char tmp[128];
	snprintf(tmp, sizeof(tmp), "XILINX_SPEEDGRADE=%d ", speed);

	//Generate the `define flags separately (since we have to coalesce them)
	for(auto f : flags)
	{
		if(f.GetType() != BuildFlag::TYPE_DEFINE)
			continue;
		defines += f.GetFlag() + "=" + f.GetArgs() + " ";
	}
	fprintf(fp, "-define {%s}\n", defines.c_str());

	for(auto f : flags)
	{
		//Ignore any non-synthesis flags
		if(!f.IsUsedAt(BuildFlag::SYNTHESIS_TIME))
			continue;

		//Ignore any hardware/ or define/ flags as those were already processed
		if(f.GetType() == BuildFlag::TYPE_HARDWARE)
			continue;
		if(f.GetType() == BuildFlag::TYPE_DEFINE)
			continue;

		//Convert the meta-flag and write it verbatim
		string fflag = FlagToStringForSynthesis(f);
		fprintf(fp, "%s\n", fflag.c_str());
	}
	fclose(fp);

	//Launch XST
	string cmdline = m_binpath + "/xst -intstyle xflow -ifn " + xst_file + " -ofn " + report_file;
	string output;
	bool ok = (0 == ShellCommand(cmdline, output));

	//Regardless of if results were successful or not, crunch the stdout log and print errors/warnings to client
	CrunchSynthesisLog(output, stdout);

	//Upload generated outputs (the .ngc and .syr are the interesting bits).
	//Note that in case of a synthesis error the NGC may not exist.
	if(DoesFileExist(ngc_file))
		outputs[ngc_file] = sha256_file(ngc_file);
	outputs[report_file] = sha256_file(report_file);

	//Done
	return ok;
}

/**
	@brief Read through the report and figure out what's interesting
 */
void XilinxISEToolchain::CrunchSynthesisLog(const string& log, string& stdout)
{
	//Split the report up into lines
	vector<string> lines;
	ParseLines(log, lines);

	for(auto line : lines)
	{
		//TODO: Blacklist messages of no importance

		//Filter out errors and warnings
		if(line.find("ERROR:") == 0)
			stdout += line + "\n";
		if(line.find("WARNING:") == 0)
			stdout += line + "\n";
	}
}

/**
	@brief Converts a BuildFlag to an XST flag
 */
string XilinxISEToolchain::FlagToStringForSynthesis(BuildFlag flag)
{
	if(flag.GetType() == BuildFlag::TYPE_OPTIMIZE)
	{
		if(flag.GetArgs() == "none")
			return "-opt_level 0";
		else if(flag.GetArgs() == "speed")
			return "-opt_level 1\n-opt_mode speed";
		else
		{
			LogWarning("Don't know what to do with optimization flag %s\n", static_cast<string>(flag).c_str());
			return "";
		}
	}

	else
	{
		//Anything else isn't yet supported
		//fprintf(fp, "%s", flags[i]->toString(this).c_str());
		LogWarning("Don't know what to do with flag %s\n", static_cast<string>(flag).c_str());
		return "";
	}
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
