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
	string arch;
	#if __x86_64__
		arch = "lin64";
	#else
		arch = "lin";
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

	//Check how many source files we have. See if we have a UCF and/or NGC as inputs.
	string src = *sources.begin();
	bool single_file = (sources.size() == 1);
	bool double_file = (sources.size() == 2);
	bool found_ucf = false;
	bool found_ngc = false;
	for(auto f : sources)
	{
		if(f.find(".ucf") != string::npos)
			found_ucf = true;
		if(f.find(".ngc") != string::npos)
			found_ngc = true;
	}

	//If we have two sources (a ucf and a ngc) we're map+PARing
	if(double_file && found_ucf && found_ngc )
		return MapAndPar(triplet, sources, fname, flags, outputs, stdout);

	//If we have only one source and it's a .ncd we're bitgen'ing.
	else if(single_file && (src.find(".ncd") != string::npos) )
		return GenerateBitstream(triplet, sources, fname, flags, outputs, stdout);

	//Assume synthesis by default
	else
		return Synthesize(triplet, sources, fname, flags, outputs, stdout);
}

/**
	@brief Parses hardware/ flags to find the target part
 */
bool XilinxISEToolchain::GetTargetPartName(
	set<BuildFlag> flags,
	string& triplet,
	string& part,
	string& stdout,
	string& fname,
	int& speed)
{
	//Go over our flags and figure out our speed grade and package
	speed = 0;
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
	part = sdev;

	return true;
}

/**
	@brief Filter a report and look for lines of interest
 */
void XilinxISEToolchain::CrunchLog(const string& log, const std::vector<std::string>& blacklist, string& stdout)
{
	//Split the report up into lines
	vector<string> lines;
	ParseLines(log, lines);

	for(size_t i=0; i<lines.size(); i++)
	{
		string line = lines[i];

		//Blacklist messages of no importance
		bool black = false;
		for(auto search : blacklist)
		{
			if(line.find(search) != string::npos)
				black = true;
		}
		if(black)
			continue;

		//Pretty-print $display messages
		if(line.find("$display") != string::npos)
		{
			//Extract file name (and fall back to pass-through if it's malformed)
			if(line[0] != '\"')
			{
				stdout += line + "\n";
				continue;
			}
			size_t epos = line.find('\"', 1);
			if(epos == string::npos)
			{
				stdout += line + "\n";
				continue;
			}
			string fname = line.substr(1, epos-1);

			//Extract line number (everything from the quote until the $display)
			size_t lstart = epos + 2;
			size_t lend = line.find('$', lstart);
			if( (lstart == string::npos) || (lend == string::npos))
			{
				stdout += line + "\n";
				continue;
			}
			string lnum = line.substr(lstart, lend-lstart-1);

			//Extract the actual message being printed
			string msg = line.substr(lend + string("$display ").length());

			//Now that we have the message, reformat it depending on the type of message
			if(msg.find("ERROR:") == 0)
			{
				stdout += string("ERROR:HDLCfg:1 - \"") + fname + " " + lnum + " " +
					msg.substr(string("ERROR: ").length()) + "\n";
			}
			else if(msg.find("WARNING:") == 0)
			{
				stdout += string("WARNING:HDLCfg:2 - \"") + fname + " " + lnum + " " +
					msg.substr(string("WARNING: ").length()) + "\n";
			}
			else
				stdout += string("INFO:HDLCfg:3 - \"") + fname + " " + lnum + " " + msg + "\n";

			continue;
		}

		//Skip anything that isn't an error or warning
		if( (line.find("ERROR:") != 0) && (line.find("WARNING:") != 0) )
			continue;

		//If subsequent lines begin with 3 spaces, they're part of this message so merge them
		string wtext = line;
		while(i+1 < lines.size())
		{
			string nline = lines[i+1];
			if(nline.find("   ") != 0)
				break;

			i++;
			wtext += " " + nline.substr(3);
		}
		stdout += wtext + "\n";
	}

}

/**
	@brief Read through the report and figure out what's interesting
 */
void XilinxISEToolchain::CrunchSynthesisLog(const string& log, string& stdout)
{
	//List of messages we do NOT want to see
	static vector<string> blacklist(
	{
		"/devlib/verilog/src/unimacro/",	//Skip warnings in Xilinx library sources

		//Disable warnings related to use of custom verilog attributes
		"WARNING:Xst:37",					//Unknown constraint, not supported

		//Disable warnings which are common/unavoidable in parameterized code
		"WARNING:Xst:638",					//conflict on KEEP property (signal is not optimized anyway)
		"WARNING:Xst:647",					//Input signal not used / optimized out
		"WARNING:Xst:653",					//Signal used but never assigned (lots of false positives)
		"WARNING:HDLCompiler:1127",			//Assignment ignored since identifier is never used
		"WARNING:Xst:1293",					//FF/latch has constant value
		"WARNING:Xst:1336",					//more than 100% of device used
											//Map optimization will sometimes make a borderline design fit
											//If it does not fit, we get an error during map so dont warn twice
		"WARNING:Xst:1426",					//init values hinder constant cleaning - usually intentional
		"WARNING:Xst:1710",					//FF/latch without init value has constant value
		"WARNING:Xst:1895",					//FF/latch without init value has constant value due to trimming
		"WARNING:Xst:1896",					//FF/latch has constant value due to trimming
		"WARNING:Xst:2404",					//FF/latch has constant value
		"WARNING:Xst:2677",					//Value not used
		"WARNING:Xst:2935",					//Signal is tied to initial value (constant outputs cause this)
		"WARNING:Xst:2999",					//Signal is tied to initial value (inferred ROMs cause this)
		"WARNING:Xst:2254",					//Area constraint not met (happens even if design fits when we're borderline)
		"WARNING:Xst:1898",					//Signal unconnected due to constant pushing (optimization)

		"ERROR:HDLCompiler:598"			//"module ignored due to errors" - we already know there are errors, STFU!
	});

	CrunchLog(log, blacklist, stdout);
}

/**
	@brief Read through the report and figure out what's interesting
 */
void XilinxISEToolchain::CrunchTranslateLog(const string& log, string& stdout)
{
	//List of messages we do NOT want to see
	static vector<string> blacklist(
	{
		"WARNING:NgdBuild:452",				//Logical net has no load (something got optimized out)
		"WARNING:NgdBuild:483",				//Attribute INIT is on the wrong type of object
											//This can be caused by cross-hierarchy optimization

		"_reconfig was distributed to a",	//Hide warnings about partial reconfiguration clocks being used
											//for PLL inputs, because they're not actually driving the PLL
	});

	CrunchLog(log, blacklist, stdout);
}

/**
	@brief Read through the report and figure out what's interesting
 */
void XilinxISEToolchain::CrunchMapLog(const string& log, string& stdout)
{
	//List of messages we do NOT want to see
	static vector<string> blacklist(
	{
		"WARNING:Timing:3223",	//Timing constraint ignored (empty time group, etc)
		"WARNING:Pack:2949",	//default DQS_BIAS ignored
								//see http://forums.xilinx.com/t5/Implementation/DQS-BIAS-report-durning-implementation/td-p/350011
		"WARNING:MapLib:41",	//All members of TNM group optimized out
		"WARNING:MapLib:50",	//Timing group optimized out
		"Place:838"				//Multiple I/O standards in bus (unavoidable when using pmods etc)
	});

	CrunchLog(log, blacklist, stdout);
}

/**
	@brief Read through the report and figure out what's interesting
 */
void XilinxISEToolchain::CrunchParLog(const string& log, string& stdout)
{
	//List of messages we do NOT want to see
	static vector<string> blacklist(
	{
		"WARNING:Par:283",					//Loadless signals, kinda obvious if we get this
		"WARNING:Par:545",					//Multithreading not supported if no timing constraints
		"WARNING:Timing:3223",				//Timing constraint ignored (empty time group, etc)
		"RAMD_D1_O has no load",			//RAM32M with unused output
		"RAMD_O has no load",				//RAM64M with unused output
		"Place:838"							//Multiple I/O standards in bus (unavoidable when using pmods etc)
	});

	CrunchLog(log, blacklist, stdout);
}

/**
	@brief Read through the report and figure out what's interesting
 */
void XilinxISEToolchain::CrunchBitgenLog(const string& log, string& stdout)
{
	//List of messages we do NOT want to see
	static vector<string> blacklist(
	{
		"WARNING:PhysDesignRules:367",		//Loadless signal (if present, will be warned by PAR too)
		"WARNING:PhysDesignRules:2410"		//Design is using a 9kbit block RAM (duplicates other messages)
	});

	CrunchLog(log, blacklist, stdout);
}

/**
	@brief Read through the report and figure out what's interesting
 */
void XilinxISEToolchain::CrunchTimingLog(const string& log, string& stdout)
{
	//List of messages we do NOT want to see
	static vector<string> blacklist(
	{
		"WARNING:Timing:3223",		//Timing constraint ignored (probably means nothing there)
									//This is commonly caused by bidirectional cross-clock constraints
	});

	CrunchLog(log, blacklist, stdout);
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

	//LogDebug("XilinxISEToolchain::Synthesize for %s\n", fname.c_str());
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

	//Format the part name
	string device;
	int speed;
	if(!GetTargetPartName(flags, triplet, device, stdout, fname, speed))
		return false;

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
	if(triplet.find("spartan3-") == 0)
		defines += "XILINX_FPGA XILINX_SPARTAN3 ";
	else if(triplet.find("spartan6-") == 0)
		defines += "XILINX_FPGA XILINX_SPARTAN6 ";
	else if(triplet.find("artix7-") == 0)
		defines += "XILINX_FPGA XILINX_7SERIES XILINX_ARTIX7 ";
	else if(triplet.find("kintex7-") == 0)
		defines += "XILINX_FPGA XILINX_7SERIES XILINX_KINTEX7 ";
	else if(triplet.find("virtex7-") == 0)
		defines += "XILINX_FPGA XILINX_7SERIES XILINX_VIRTEX7 ";
	else if(triplet.find("zynq7-") == 0)
		defines += "XILINX_FPGA XILINX_7SERIES XILINX_ZYNQ7 ";
	else if(triplet.find("coolrunner2-") == 0)
		defines += "XILINX_CPLD XILINX_COOLRUNNER2 ";
	else
	{
		stdout += string("ERROR: Don't know what to do with device ") + device + " (" + triplet + ")\n";
		fclose(fp);
		return false;
	}
	char tmp[128];
	snprintf(tmp, sizeof(tmp), "XILINX_SPEEDGRADE=%d ", speed);
	defines += tmp;

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
	@brief Converts a BuildFlag to an XST flag
 */
string XilinxISEToolchain::FlagToStringForSynthesis(BuildFlag flag)
{
	//Ignore any non-map flags
	if(!flag.IsUsedAt(BuildFlag::SYNTHESIS_TIME))
		return "";

	if(flag.GetType() == BuildFlag::TYPE_OPTIMIZE)
	{
		if(flag.GetFlag() == "none")
			return "-opt_level 0\n-opt_mode speed";
		else if(flag.GetFlag() == "speed")
			return "-opt_level 1\n-opt_mode speed";
		else if(flag.GetFlag() == "hierarchy")
		{
			string args = flag.GetArgs();
			string sargs;
			if(args == "flatten")
				sargs = "no";
			else if(args == "keep")
				sargs = "yes";
			else if(args == "synth_only")
				sargs = "soft";
			else
			{
				LogWarning("Don't know what to do with optimization hierarchy specifier %s\n",
					static_cast<string>(flag).c_str());
					return "";
			}
			return string("-keep_hierarchy ") + sargs;
		}
		else
		{
			LogWarning("Don't know what to do with optimization flag %s\n",
				static_cast<string>(flag).c_str());
			return "";
		}
	}

	else if(flag.GetType() == BuildFlag::TYPE_OUTPUT)
	{
		if(flag.GetFlag() == "hierarchy")
		{
			string args = flag.GetArgs();
			string sargs;
			if(args == "unchanged")
				sargs = "as_optimized";
			else if(args == "rebuild")
				sargs = "rebuilt";
			else
			{
				LogWarning("Don't know what to do with output hierarchy specifier %s\n",
					static_cast<string>(flag).c_str());
					return "";
			}
			return string("-netlist_hierarchy ") + sargs;
		}
		else
		{
			LogWarning("Don't know what to do with output flag %s\n",
				static_cast<string>(flag).c_str());
			return "";
		}
	}

	else if(flag.GetType() == BuildFlag::TYPE_WARNING)
	{
		//we default to max warning level (absurdly verbose)
		if(flag.GetFlag() == "max")
			return "";

		else
		{
			LogWarning("Don't know what to do with warning flag %s\n",
				static_cast<string>(flag).c_str());
			return "";
		}
	}

	else if(flag.GetType() == BuildFlag::TYPE_LIBRARY)
	{
		//Specify include directory
		if(flag.GetFlag() == "__incdir")
			return string("-vlgincdir ") + flag.GetArgs();

		else
		{
			LogWarning("Don't know what to do with warning flag %s\n",
				static_cast<string>(flag).c_str());
			return "";
		}
	}

	else
	{
		//Anything else isn't yet supported
		LogWarning("XilinxISEToolchain::Synthesize: Don't know what to do with flag %s\n", static_cast<string>(flag).c_str());
		return "";
	}
}

/**
	@brief Converts a BuildFlag to a ngdbuild flag

	TODO:
		-a (add pads to top level signals)
		-aul (allow unmatched LOCs)
		-aut (allow unmatched timegroups)
		-bm (block ram files)
		-insert_keep_hierarchy (add keep hierarchy constraints)
		-r (ignore LOCs)
		-u (allow unexpanded blocks)
		-verbose (lots of spam)
 */
string XilinxISEToolchain::FlagToStringForTranslate(BuildFlag flag)
{
	//Ignore any non-map flags
	if(!flag.IsUsedAt(BuildFlag::MAP_TIME))
		return "";

	if(flag.GetType() == BuildFlag::TYPE_OPTIMIZE)
	{
		//Silently ignore all optimization flags (we don't do optimization at the translate phase)
		return "";
	}

	else if(flag.GetType() == BuildFlag::TYPE_HARDWARE)
	{
		//Silently ignore all hardware flags (target device is already baked into the netlist)
		return "";
	}

	else if(flag.GetType() == BuildFlag::TYPE_WARNING)
	{
		//we default to max warning level (absurdly verbose)
		if(flag.GetFlag() == "max")
			return "";

		else
		{
			LogWarning("Don't know what to do with warning flag %s\n",
				static_cast<string>(flag).c_str());
			return "";
		}
	}

	else
	{
		//Anything else isn't yet supported
		LogWarning("XilinxISEToolchain::Translate: Don't know what to do with flag %s\n", static_cast<string>(flag).c_str());
		return "";
	}
}

/**
	@brief Converts a BuildFlag to a map flag

	TODO:
	-activityfile		(SAIF activity file for power etc optimization)
	-bp					(map slice logic to unused BRAMs)
	-c					(slice packing density)

	optimize/speed:				-logic_opt on
	optimize/none:				-logic_opt off

	optimize/quick:				-ol std

	optimize/pack/size:			-c 1
	optimize/pack/speed:		-c 100

	optimize/regmerge:			-equivalent_register_removal on
	optimize/noregmerge:		-equivalent_register_removal off

	optimize/regsplit:			-register_duplication on
	optimize/noregsplit:		-register_duplication off

	optimize/regpack/none:		-r off
	optimize/regpack/single:	-r 4
	optimize/regpack/double:	-r 8

	optimize/global/speed:		-global_opt speed
	optimize/global/size:		-global_opt area

	optimize/lutmerge/size:		-lc area
	optimize/lutmerge/balanced:	-lc auto
	optimize/lutmerge/speed:	-lc off

	optimize/iodff/input		-pr i
	optimize/iodff/output		-pr o
	optimize/iodff/all			-pr b
	optimize/iodff/none			-pr off

	TODO: power optimization options
	TODO: smartguide
	TODO: map seeds (cost tables)
 */
string XilinxISEToolchain::FlagToStringForMap(BuildFlag flag)
{
	//Ignore any non-map flags
	if(!flag.IsUsedAt(BuildFlag::MAP_TIME))
		return "";

	if(flag.GetType() == BuildFlag::TYPE_OPTIMIZE)
	{
		LogWarning("XilinxISEToolchain::Map: Don't know what to do with optimization flag %s\n",
			static_cast<string>(flag).c_str());
		return "";
	}

	else if(flag.GetType() == BuildFlag::TYPE_HARDWARE)
	{
		//Silently ignore all hardware flags (target device is already baked into the netlist)
		return "";
	}

	else if(flag.GetType() == BuildFlag::TYPE_WARNING)
	{
		//we default to max warning level (absurdly verbose)
		if(flag.GetFlag() == "max")
			return "";

		else
		{
			LogWarning("XilinxISEToolchain::Map: Don't know what to do with warning flag %s\n",
				static_cast<string>(flag).c_str());
			return "";
		}
	}

	else
	{
		//Anything else isn't yet supported
		LogWarning("XilinxISEToolchain::Map: Don't know what to do with flag %s\n", static_cast<string>(flag).c_str());
		return "";
	}
}

/**
	@brief Converts a BuildFlag to a PAR flag

	optimize/quick:			-ol std -rl std
	default:				-ol high -rl high

	optimize/power:			-power on
 */
string XilinxISEToolchain::FlagToStringForPAR(BuildFlag flag)
{
	//Ignore any non-PAR flags
	if(!flag.IsUsedAt(BuildFlag::PAR_TIME))
		return "";

	if(flag.GetType() == BuildFlag::TYPE_OPTIMIZE)
	{
		LogWarning("XilinxISEToolchain::PAR: Don't know what to do with optimization flag %s\n",
			static_cast<string>(flag).c_str());
		return "";
	}

	else if(flag.GetType() == BuildFlag::TYPE_HARDWARE)
	{
		//Silently ignore all hardware flags (target device is already baked into the netlist)
		return "";
	}

	else if(flag.GetType() == BuildFlag::TYPE_WARNING)
	{
		//we default to max warning level (absurdly verbose)
		if(flag.GetFlag() == "max")
			return "";

		else
		{
			LogWarning("XilinxISEToolchain::PAR: Don't know what to do with warning flag %s\n",
				static_cast<string>(flag).c_str());
			return "";
		}
	}

	else
	{
		//Anything else isn't yet supported
		LogWarning("XilinxISEToolchain::PAR: Don't know what to do with flag %s\n", static_cast<string>(flag).c_str());
		return "";
	}
}

/**
	@brief Converts a BuildFlag to a bitgen flag

	output/compress			-g Compress
	output/bootfallback		-g ConfigFallBack
	output/ConfigRate/x		-g ConfigRate:x
	output/debugbit			-g DebugBitstream:Yes
	output/jtaglock			-g DISABLE_JTAG:Yes
	output/drivedone		-g DriveDone:Yes
	output/thermalshutdown	-g XADCPowerDown:Enable
	output/readback			-g ReadBack
	output/readlock			-g Security:Level1
	output/reconfiglock		-g Security:Level2
	output/spiwidth/x		-g SPI_buswidth:x
	output/spinegclk		-g SPI_Fall_Edge
	output/unused/pulldown	-g UnusedPin:PullDown
	output/unused/pullup	-g UnusedPin:PullUp
	output/unused/float_t	-g UnusedPin:PullNone
	output/usercode/$HEX	-g UserID:$HEX
	output/userintcode/$HEX	-g USR_ACCESS:$HEX

	TODO: bitfile encryption stuff
 */
string XilinxISEToolchain::FlagToStringForBitgen(BuildFlag flag)
{
	//Ignore any non-bitgen flags
	if(!flag.IsUsedAt(BuildFlag::IMAGE_TIME))
		return "";

	if(flag.GetType() == BuildFlag::TYPE_OPTIMIZE)
	{
		LogWarning("XilinxISEToolchain::GenerateBitstream: Don't know what to do with optimization flag %s\n",
			static_cast<string>(flag).c_str());
		return "";
	}

	else if(flag.GetType() == BuildFlag::TYPE_HARDWARE)
	{
		//Silently ignore all hardware flags (target device is already baked into the netlist)
		return "";
	}

	else if(flag.GetType() == BuildFlag::TYPE_WARNING)
	{
		//we default to max warning level (absurdly verbose)
		if(flag.GetFlag() == "max")
			return "";

		else
		{
			LogWarning("XilinxISEToolchain::GenerateBitstream: Don't know what to do with warning flag %s\n",
				static_cast<string>(flag).c_str());
			return "";
		}
	}

	else
	{
		//Anything else isn't yet supported
		LogWarning("XilinxISEToolchain::GenerateBitstream: Don't know what to do with flag %s\n", static_cast<string>(flag).c_str());
		return "";
	}
}

/**
	@brief Converts a BuildFlag to a trce flag
 */
string XilinxISEToolchain::FlagToStringForTiming(BuildFlag flag)
{
	//Ignore any non-trce flags
	if(!flag.IsUsedAt(BuildFlag::ANALYSIS_TIME))
		return "";

	if(flag.GetType() == BuildFlag::TYPE_OPTIMIZE)
	{
		//ignore, it's way too late for optimization now
		return "";
	}

	else if(flag.GetType() == BuildFlag::TYPE_HARDWARE)
	{
		//Silently ignore all hardware flags (target device is already baked into the netlist)
		return "";
	}

	else if(flag.GetType() == BuildFlag::TYPE_WARNING)
	{
		//we default to max warning level (absurdly verbose)
		if(flag.GetFlag() == "max")
			return "";

		else
		{
			LogWarning("XilinxISEToolchain::StaticTiming: Don't know what to do with warning flag %s\n",
				static_cast<string>(flag).c_str());
			return "";
		}
	}

	else
	{
		//Anything else isn't yet supported
		LogWarning("XilinxISEToolchain::StaticTiming: Don't know what to do with flag %s\n", static_cast<string>(flag).c_str());
		return "";
	}
}

/**
	@brief Helper function to call Translate(), Map(), Par(), and StaticTiming()
 */
bool XilinxISEToolchain::MapAndPar(
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	if(!Translate(triplet, sources, fname, flags, outputs, stdout))
		return false;
	if(!Map(triplet, sources, fname, flags, outputs, stdout))
		return false;
	if(!Par(triplet, sources, fname, flags, outputs, stdout))
		return false;
	return StaticTiming(triplet, sources, fname, flags, outputs, stdout);
}

/**
	@brief Translation from NGC+UCF to NGD
 */
bool XilinxISEToolchain::Translate(
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	stdout = "";

	//Get NGD filename from NCD filename
	string base = GetBasenameOfFileWithoutExt(fname);
	fname = base + ".ngd";
	string ngd_file = base + ".ngd";
	string report_file = base + ".bld";
	//LogDebug("XilinxISEToolchain::Translate for %s\n", fname.c_str());

	//Format the part name
	string device;
	int speed;
	if(!GetTargetPartName(flags, triplet, device, stdout, fname, speed))
		return false;

	//Verify we have two sources, a UCF and a NGC. Find them.
	string ucf;
	string ngc;
	for(auto s : sources)
	{
		if(s.find(".ucf") != string::npos)
			ucf = s;
		else if(s.find(".ngc") != string::npos)
			ngc = s;
		else
		{
			stdout += string("ERROR: XilinxISEToolchain got invalid input file ") + s + "\n";
			return false;
		}
	}
	if(ucf == "")
	{
		stdout += "ERROR: XilinxISEToolchain needs a constraint (.ucf) file\n";
		return false;
	}
	if(ngc == "")
	{
		stdout += "ERROR: XilinxISEToolchain needs a netlist (.ngc) file\n";
		return false;
	}

	//Format flags
	string sflags;
	for(auto f : flags)
		sflags += FlagToStringForTranslate(f);

	//Launch ngdbuild
	string cmdline = m_binpath + "/ngdbuild -intstyle xflow -dd ngdbuild_tmp -nt on -p " + device + " " +
						sflags + " " + " -uc " + ucf + " " + ngc;
	string output;
	bool ok = (0 == ShellCommand(cmdline, output));

	//Regardless of if results were successful or not, crunch the stdout log and print errors/warnings to client
	CrunchTranslateLog(output, stdout);

	//Upload generated outputs (the .ngd and .bld are the interesting bits).
	//Note that in case of a translation error the NGD may not exist.
	if(DoesFileExist(ngd_file))
		outputs[ngd_file] = sha256_file(ngd_file);
	outputs[report_file] = sha256_file(report_file);

	//Done
	return ok;
}

/**
	@brief Technology mapping and initial placement
 */
bool XilinxISEToolchain::Map(
	string triplet,
	set<string> /*sources*/,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	//Get mapped filenames from NCD filename
	string base = GetBasenameOfFileWithoutExt(fname);
	fname = base + "_map.ncd";
	string ngd_file = base + ".ngd";
	string pcf_file = base + ".pcf";
	string report_file = base + "_map.mrp";
	string report_file2 = base + "_map.map";
	//LogDebug("XilinxISEToolchain::Map for %s\n", fname.c_str());

	//Format the part name
	string device;
	int speed;
	if(!GetTargetPartName(flags, triplet, device, stdout, fname, speed))
		return false;

	//Format flags
	string sflags;
	for(auto f : flags)
		sflags += FlagToStringForMap(f);

	//TODO: Multithreading (-mt off|2) based on the job's focus on throughput or latency
	//Multithreading not supported for spartan-3 so always leave it off there

	//Launch map
	//TODO: flags
	string cmdline = m_binpath + "/map -intstyle xflow -detail -ir off -p " + device + " " + sflags + " -o " +
		fname + " " + ngd_file + " " + pcf_file;
	string output;
	bool ok = (0 == ShellCommand(cmdline, output));

	//Regardless of if results were successful or not, crunch the stdout log and print errors/warnings to client
	CrunchMapLog(output, stdout);

	//Upload generated outputs (the .ncd, .pcf, .mrp, and .map are the interesting bits).
	//Note that in case of a mapping error some files may not exist
	if(DoesFileExist(fname))
		outputs[fname] = sha256_file(fname);
	if(DoesFileExist(pcf_file))
		outputs[pcf_file] = sha256_file(pcf_file);
	if(DoesFileExist(report_file))
		outputs[report_file] = sha256_file(report_file);
	if(DoesFileExist(report_file2))
		outputs[report_file2] = sha256_file(report_file2);

	//Done
	return ok;
}

/**
	@brief Routing
 */
bool XilinxISEToolchain::Par(
	string triplet,
	set<string> /*sources*/,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	//Get mapped filenames from NCD filename
	string base = GetBasenameOfFileWithoutExt(fname);
	string map_file = base + "_map.ncd";
	string pcf_file = base + ".pcf";
	string report_file = base + ".par";
	string report_file2 = base + ".unroutes";
	//LogDebug("XilinxISEToolchain::Par for %s\n", fname.c_str());

	//Format the part name
	string device;
	int speed;
	if(!GetTargetPartName(flags, triplet, device, stdout, fname, speed))
		return false;

	//Format flags
	string sflags;
	for(auto f : flags)
		sflags += FlagToStringForPAR(f);

	//TODO: Multithreading (-mt off|2|3|4) based on the job's focus on throughput or latency
	//Multithreading not supported for spartan-3 so always leave it off there

	//Launch par
	//TODO: flags
	string cmdline = m_binpath + "/par -intstyle xflow " + sflags + " " + map_file + " " + fname + " " + pcf_file;

	string output;
	bool ok = (0 == ShellCommand(cmdline, output));

	//Regardless of if results were successful or not, crunch the stdout log and print errors/warnings to client
	CrunchParLog(output, stdout);

	//Upload generated outputs (the .ncd, .par, .unroutes are the interesting bits).
	//Note that in case of a mapping error some files may not exist
	if(DoesFileExist(fname))
		outputs[fname] = sha256_file(fname);
	if(DoesFileExist(report_file))
		outputs[report_file] = sha256_file(report_file);
	if(DoesFileExist(report_file2))
		outputs[report_file2] = sha256_file(report_file2);

	//Done
	return ok;
}

/**
	@brief Static timing analysis
 */
bool XilinxISEToolchain::StaticTiming(
	string /*triplet*/,
	set<string> /*sources*/,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	//Get TWX filename from NCD filename
	string base = GetBasenameOfFileWithoutExt(fname);
	string ncd_file = base + ".ncd";
	string report_file = base + ".twr";
	string pcf_file = base + ".pcf";
	string twx_file = base + ".twx";
	//LogDebug("XilinxISEToolchain::StaticTiming for %s\n", fname.c_str());

	//Launch trce
	//Figure out flags
	string sflags;
	for(auto f : flags)
		sflags += FlagToStringForTiming(f);
	string cmdline = m_binpath + "/trce -v 10 -intstyle xflow -l 10 -fastpaths " + sflags + " " +
		ncd_file + " " + pcf_file + " -o " + report_file + " -xml " + twx_file;
	string output;
	bool ok = (0 == ShellCommand(cmdline, output));

	//Regardless of if results were successful or not, crunch the stdout log and print errors/warnings to client
	CrunchTimingLog(output, stdout);

	//Upload generated outputs (the .twx and .twr are the interesting bits).
	//Note that in case of an arg parsing error etc the TWX may not exist.
	if(DoesFileExist(twx_file))
		outputs[twx_file] = sha256_file(twx_file);
	outputs[report_file] = sha256_file(report_file);

	//Done
	return ok;
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
	stdout = "";

	//Look up the source file name
	string ncd_file = *sources.begin();

	//Get other filenames from BIT filename
	string base = GetDirOfFile(ncd_file) + "/" + GetBasenameOfFileWithoutExt(ncd_file);
	string report_file = base + ".bgn";
	//LogDebug("XilinxISEToolchain::GenerateBitstream for %s\n", fname.c_str());

	//Figure out flags
	string sflags = "-intstyle xflow -g DonePipe:Yes ";
	if(triplet.find("spartan6-") == 0)
		sflags += "-g INIT_9L:Yes ";
	for(auto f : flags)
		sflags += FlagToStringForBitgen(f);

	//Launch bitgen
	string cmdline = m_binpath + "/bitgen " + sflags + " " + ncd_file + " " + fname;
	string output;
	bool ok = (0 == ShellCommand(cmdline, output));

	//Regardless of if results were successful or not, crunch the stdout log and print errors/warnings to client
	CrunchBitgenLog(output, stdout);

	//Upload generated outputs (the .bit and .bgn are the interesting bits).
	//Note that in case of an arg parsing error etc the BIT may not exist.
	if(DoesFileExist(fname))
		outputs[fname] = sha256_file(fname);
	if(DoesFileExist(report_file))
		outputs[report_file] = sha256_file(report_file);

	//Done
	return ok;
}
