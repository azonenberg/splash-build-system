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

YosysToolchain::YosysToolchain(string basepath)
	: FPGAToolchain(basepath, TOOLCHAIN_YOSYS)
{
	//Save version info
	string sver = ShellCommand(basepath + " -V");
	sscanf(sver.c_str(), "Yosys %8d.%8d+%8d", &m_majorVersion, &m_minorVersion, &m_patchVersion);

	char tmp[128];
	snprintf(tmp, sizeof(tmp), "Yosys %d.%d+%d", m_majorVersion, m_minorVersion, m_patchVersion);
	m_stringVersion = tmp;

	//Set list of target architectures
	FindArchitectures();

	//File format suffixes
	m_fixes["netlist"] = stringpair("", ".json");
	m_fixes["formal-netlist"] = stringpair("", ".smt2");
	m_fixes["formal"] = stringpair("", ".txt");
	m_fixes["bitstream"] = stringpair("", ".txt");
	m_fixes["constraint"] = stringpair("", ".pcf");

	//Generate the hash based on the full git ID etc
	m_hash = sha256(sver);
}

YosysToolchain::~YosysToolchain()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Search for other toolchains

void YosysToolchain::FindArchitectures()
{
	//We always support formal targets b/c those don't need any extra tools
	m_triplets.emplace("generic-formal");

	//Get our search path
	vector<string> dirs;
	ParseSearchPath(dirs);

	//TODO: IceStorm stuff

	//Look for gp4par
	m_gp4parPath = FindExecutable("gp4par", dirs);
	if(m_gp4parPath != "")
	{
		m_triplets.emplace("greenpak4-slg46140");
		m_triplets.emplace("greenpak4-slg46620");
		m_triplets.emplace("greenpak4-slg46621");
	}
}

/**
	@brief Look for a binary anywhere in the search path
 */
string YosysToolchain::FindExecutable(string fname, vector<string>& dirs)
{
	for(auto dir : dirs)
	{
		string path = dir + "/" + fname;
		if(DoesFileExist(path))
			return path;
	}

	return "";
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Toolchain properties

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual compilation

bool YosysToolchain::Build(
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	//Switch on the triplet to decide how to handle it
	if(triplet == "generic-formal")
	{
		//If the output file is a .smt2 we're building for the model checker
		if(fname.find(".smt2") != string::npos)
			return BuildFormal(triplet, sources, fname, flags, outputs, stdout);

		//Otherwise we're running the checker
		else
			return CheckModel(triplet, sources, fname, flags, outputs, stdout);
	}
	else if(triplet.find("greenpak4-") == 0)
	{
		//If the output file is a .json we're synthesizing
		if(fname.find(".json") != string::npos)
			return SynthGreenPAK(triplet, sources, fname, flags, outputs, stdout);

		//Otherwise running gp4par
		else
			return PARGreenPAK(triplet, sources, fname, flags, outputs, stdout);
	}
	else
	{
		stdout = string("ERROR: YosysToolchain doesn't know how to build for architecture ") + triplet + "\n";
		return false;
	}
}

/**
	@brief Synthesize for formal verification
 */
bool YosysToolchain::BuildFormal(
	string /*triplet*/,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	stdout = "";

	//LogDebug("YosysToolchain::BuildFormal for %s\n", fname.c_str());
	string base = GetBasenameOfFileWithoutExt(fname);

	//Flags
	string incflags = "";
	for(auto f : flags)
	{
		if(f.GetType() == BuildFlag::TYPE_LIBRARY)
		{
			if(f.GetFlag() == "__incdir")
				incflags += "-I" + f.GetArgs() + " ";
		}

		else
		{
			stdout += string("WARNING: YosysToolchain::BuildFormal: Don't know what to do with flag ") +
				static_cast<string>(f) + "\n";
		}
	}

	//Write the synthesis script
	string ys_file = base + ".ys";
	string smt_file = base + ".smt2";
	FILE* fp = fopen(ys_file.c_str(), "w");
	if(!fp)
	{
		stdout += string("ERROR: Failed to create synthesis script ") + ys_file + "\n";
		return false;
	}
	for(auto s : sources)
	{
		//Ignore any non-Verilog sources
		if(s.find(".v") != string::npos)
			fprintf(fp, "read_verilog -formal %s \"%s\"\n", incflags.c_str(), s.c_str());
	}
	fprintf(fp, "prep -nordff -top %s\n", base.c_str());
	fprintf(fp, "check -assert\n");
	fprintf(fp, "write_smt2 -wires %s", smt_file.c_str());
	fclose(fp);

	//Run synthesis
	string report_file = base + ".log";
	string cmdline = m_basepath + " -t -l " + report_file + " " + ys_file;
	string output;
	bool ok = (0 == ShellCommand(cmdline, output));

	//Crunch the synthesis log
	CrunchYosysLog(output, stdout);

	//Done, return results
	if(DoesFileExist(report_file))
		outputs[report_file] = sha256_file(report_file);
	if(DoesFileExist(smt_file))
		outputs[smt_file] = sha256_file(smt_file);
	else
	{
		stdout += "ERROR: No SMT file produced\n";
		return false;
	}

	return ok;
}

/**
	@brief Synthesize for formal verification
 */
bool YosysToolchain::CheckModel(
	string /*triplet*/,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	stdout = "";

	//TODO: Separate options to run base case and temporal induction??

	//Look up the constraints and source file
	string smt_file;
	string constraints_file;
	for(auto s : sources)
	{
		if(s.find(".smtc") != string::npos)
			constraints_file = s;
		else if(s.find(".smt2") != string::npos)
			smt_file = s;
		else
		{
			stdout = "ERROR: YosysToolchain::CheckModel: Don't know what to do with " + s  + "\n";
			return false;
		}
	}

	if(smt_file == "")
	{
		stdout = "ERROR: YosysToolchain: no SMT file specified\n";
		return false;
	}

	string report_file = GetBasenameOfFile(fname);

	//Format the bulk of the command line (beginning and end vary)
	string num_steps = "20";
	for(auto f : flags)
	{
		//TODO: Flags

		/*
		if(f.GetType() != BuildFlag::TYPE_DEFINE)
			continue;
		defines += f.GetFlag() + "=" + f.GetArgs() + " ";
		*/
		stdout += string("WARNING: YosysToolchain::BuildFormal: Don't know what to do with flag ") +
			static_cast<string>(f) + "\n";
	}
	string basename = GetBasenameOfFileWithoutExt(fname);
	string vcd_file = basename + ".vcd";
	string cmdline_meat = "-t " + num_steps + " --dump-vcd " + vcd_file + " ";
	if(constraints_file != "")
		cmdline_meat += string("--smtc ") + constraints_file + " ";
	cmdline_meat += smt_file + " ";

	//Run the inductive step first. Counterintuitive since it's meaningless w/o the base case!
	//But it usually runs a lot faster than the base case, and the vast majority of errors can be caught here.
	//This means we can skip the time-consuming base case and iterate faster during verification.
	string cmdline = m_basepath + "-smtbmc -i " + cmdline_meat;
	string report_induction;
	bool ok = (0 == ShellCommand(cmdline, report_induction));

	//Run the base case (append to the existing report)
	cmdline = m_basepath + "-smtbmc " + cmdline_meat;
	string report_base;
	ok &= (0 == ShellCommand(cmdline, report_base));

	//Write to the report file (TODO: be able to upload this directly?)
	string report = report_induction + "\n\n\n\n\n" + report_base;
	PutFileContents(report_file, report);

	//Filter the results and print to stdout clientside
	CrunchSMTLog(report, stdout);

	//Upload the report
	outputs[report_file] = sha256_file(report_file);
	if(DoesFileExist(vcd_file))
		outputs[vcd_file] = sha256_file(vcd_file);

	//Done
	return ok;
}

bool YosysToolchain::SynthGreenPAK(
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	LogDebug("YosysToolchain::SynthGreenPAK for %s\n", fname.c_str());
	stdout = "";
	string base = GetBasenameOfFileWithoutExt(fname);

	//Look up the part
	string part = triplet.substr(triplet.find("-")+1);
	for(size_t i=0; i<part.length(); i++)
		part[i] = toupper(part[i]);
	part += "V";

	//Write the synthesis script
	string ys_file = base + ".ys";
	string json_file = base + ".json";
	FILE* fp = fopen(ys_file.c_str(), "w");
	if(!fp)
	{
		stdout += string("ERROR: Failed to create synthesis script ") + ys_file + "\n";
		return false;
	}
	for(auto s : sources)
	{
		//Ignore any non-Verilog sources
		if(s.find(".v") != string::npos)
			fprintf(fp, "read_verilog -formal \"%s\"\n", s.c_str());
	}
	fprintf(fp, "synth_greenpak4 -part %s\n", part.c_str());
	fprintf(fp, "write_json %s\n", json_file.c_str());
	for(auto f : flags)
		fprintf(fp, "%s\n", FlagToStringForSynthesis(f).c_str());
	fclose(fp);

	//Run synthesis
	string report_file = base + "_synth.log";
	string cmdline = m_basepath + " -t -l " + report_file + " " + ys_file;
	string output;
	bool ok = (0 == ShellCommand(cmdline, output));

	//Crunch the synthesis log
	CrunchYosysLog(output, stdout);

	//Done, return results
	if(DoesFileExist(report_file))
		outputs[report_file] = sha256_file(report_file);
	if(DoesFileExist(json_file))
		outputs[json_file] = sha256_file(json_file);
	else
	{
		stdout += "ERROR: No JSON file produced\n";
		return false;
	}

	return ok;
}

bool YosysToolchain::PARGreenPAK(
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	stdout = "";

	//Look up the constraints and source file
	string json_file;
	string constraints_file;
	for(auto s : sources)
	{
		if(s.find(".pcf") != string::npos)
			constraints_file = s;
		else if(s.find(".json") != string::npos)
			json_file = s;
		else
		{
			stdout = "ERROR: YosysToolchain::PARGreenPAK: Don't know what to do with " + s  + "\n";
			return false;
		}
	}

	if(json_file == "")
	{
		stdout = "ERROR: YosysToolchain: no JSON file specified\n";
		return false;
	}

	string basename = GetBasenameOfFileWithoutExt(fname);
	string bit_file = basename + ".txt";
	string report_file = basename + "_par.log";

	//Look up the part
	string part = triplet.substr(triplet.find("-")+1);
	for(size_t i=0; i<part.length(); i++)
		part[i] = toupper(part[i]);
	part += "V";

	//TODO: unused_-pull, unused-drive

	//Format flags
	string sflags = "";
	for(auto f : flags)
		sflags += FlagToStringForGP4PAR(f);

	//Format command line
	string cmdline = m_gp4parPath + " " + sflags + " --output " + bit_file + " -p " + part + " --constraints " + constraints_file
		+ " " + json_file + " --logfile " + report_file;
	cmdline += " --debug --nocolors";
	string stdout_raw;
	bool ok = (0 == ShellCommand(cmdline, stdout_raw));

	//Filter the results and print to stdout clientside
	CrunchGP4PARLog(stdout_raw, stdout);

	//Upload the report
	outputs[report_file] = sha256_file(report_file);
	if(DoesFileExist(bit_file))
		outputs[bit_file] = sha256_file(bit_file);

	//Done
	return ok;
}

string YosysToolchain::FlagToStringForGP4PAR(BuildFlag flag)
{
	//Ignore any flags we arent interested in
	if(!flag.IsUsedAt(BuildFlag::IMAGE_TIME) && !flag.IsUsedAt(BuildFlag::PAR_TIME) && !flag.IsUsedAt(BuildFlag::MAP_TIME))
		return "";

	if(flag.GetType() == BuildFlag::TYPE_OPTIMIZE)
	{
		LogWarning("YosysToolchain::FlagToStringForGP4PAR: Don't know what to do with optimization flag %s\n",
			static_cast<string>(flag).c_str());
		return "";
	}

	else if(flag.GetType() == BuildFlag::TYPE_HARDWARE)
	{
		//Silently ignore all hardware flags (target device is already baked into the netlist)
		return "";
	}

	else if(flag.GetType() == BuildFlag::TYPE_OUTPUT)
	{
		if(flag.GetFlag() == "unused")
			return "--unused-pull " + flag.GetArgs() + " ";

		else if(flag.GetFlag() == "pull")
			return "--unused-drive " + flag.GetArgs() + " ";

		else
		{
			LogWarning("YosysToolchain::FlagToStringForGP4PAR: Don't know what to do with warning flag %s\n",
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
			LogWarning("YosysToolchain::FlagToStringForGP4PAR: Don't know what to do with warning flag %s\n",
				static_cast<string>(flag).c_str());
			return "";
		}
	}

	else
	{
		//Anything else isn't yet supported
		LogWarning("YosysToolchain::FlagToStringForGP4PAR: Don't know what to do with flag %s\n", static_cast<string>(flag).c_str());
		return "";
	}
}

string YosysToolchain::FlagToStringForSynthesis(BuildFlag flag)
{
	//Ignore any flags we arent interested in
	if(!flag.IsUsedAt(BuildFlag::SYNTHESIS_TIME))
		return "";

	if(flag.GetType() == BuildFlag::TYPE_OPTIMIZE)
	{
		LogWarning("YosysToolchain::FlagToStringForSynthesis: Don't know what to do with optimization flag %s\n",
			static_cast<string>(flag).c_str());
		return "";
	}

	else if(flag.GetType() == BuildFlag::TYPE_HARDWARE)
	{
		//TODO: do we have to use anything here? Normally the triplet has all the info we need
		//(speed/package are irrelevant for synth)
		return "";
	}

	else if(flag.GetType() == BuildFlag::TYPE_WARNING)
	{
		//we default to max warning level (absurdly verbose)
		if(flag.GetFlag() == "max")
			return "";

		else
		{
			LogWarning("YosysToolchain::FlagToStringForSynthesis: Don't know what to do with warning flag %s\n",
				static_cast<string>(flag).c_str());
			return "";
		}
	}

	else
	{
		//Anything else isn't yet supported
		LogWarning("YosysToolchain::FlagToStringForSynthesis: Don't know what to do with flag %s\n", static_cast<string>(flag).c_str());
		return "";
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reporting helpers

/**
	@brief Read through the report and figure out what's interesting
 */
void YosysToolchain::CrunchYosysLog(const string& log, string& stdout)
{
	//Split the report up into lines
	vector<string> lines;
	ParseLines(log, lines);

	for(auto line : lines)
	{
		//Blacklist messages of no importance
		if(line.find("fraig_sweep") != string::npos)				//some kind of ABC message about combinatorial logic
			continue;

		//Filter out errors and warnings
		size_t istart = line.find("ERROR:");
		if(istart != string::npos)
			stdout += line.substr(istart) + "\n";

		istart = line.find("Warning:");
		if(istart != string::npos)
			stdout += line.substr(istart) + "\n";
	}
}

void YosysToolchain::CrunchGP4PARLog(const string& log, string& stdout)
{
	//Split the report up into lines
	vector<string> lines;
	ParseLines(log, lines);

	for(size_t i=0; i<lines.size(); i++)
	{
		string line = lines[i];

		//Filter out errors and warnings
		size_t istart = line.find("ERROR:");
		size_t jstart = line.find("Warning:");

		string indent;

		//Skip anything that isn't an error or warning
		if(istart != string::npos)
		{
			indent = line.substr(0, istart);
			line = line.substr(istart);
		}
		else if(jstart != string::npos)
		{
			indent = line.substr(0, jstart);
			line = line.substr(jstart);
		}
		else
			continue;

		//If subsequent lines begin with indentation, they're part of this message so merge them
		string wtext = line;
		while(i+1 < lines.size())
		{
			string nline = lines[i+1];

			//If it has a new error/warning, skip it
			if( (nline.find("ERROR:") != string::npos) || (nline.find("Warning:") != string::npos) )
				break;

			//If not same indent level, skip it
			if(nline.find(indent) != 0)
				break;

			i++;
			wtext += " " + nline.substr(indent.length());
		}
		stdout += wtext + "\n";
	}
}

void YosysToolchain::CrunchSMTLog(const string& log, string& stdout)
{
	//Split the report up into lines
	vector<string> lines;
	ParseLines(log, lines);

	bool unfilter = false;
	for(auto line : lines)
	{
		if(line.find("Assert failed") != string::npos)
			stdout += string("ERROR: ") + line + "\n";

		if(line.find("Traceback (most recent call last)") != string::npos)
		{
			stdout += "ERROR: Something went wrong (got a stack trace)\n";
			unfilter = true;
		}

		//If we stopped filtering, copy everything
		if(unfilter)
			stdout += line + "\n";
	}
}
