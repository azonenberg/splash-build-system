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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief Figure out what flags we have to pass to the compiler and linker in order to build for a specific architecture.

	An empty string indicates this is the default.

	ALL supported triplets must be listed here; the absence of an entry is an error.
 */
GNUToolchain::GNUToolchain(string arch)
{
	/////////////////////////////////////////////////////////////////////////////
	// X86

	if(arch == "x86_64-linux-gnu")
	{
		m_archflags["x86_64-linux-gnu"] 	= "-m64";
		m_archflags["x86_64-linux-gnux32"]	= "-mx32";
		m_archflags["i386-linux-gnu"]		= "-m32";
	}

	else if(arch == "x86_64-w64-mingw32")
	{
		m_archflags[arch] = "";
	}

	else if(arch == "i386-linux-gnu")
	{
		m_archflags[arch] = "";
	}

	/////////////////////////////////////////////////////////////////////////////
	// MIPS

	else if(arch == "mips-elf")
	{
		m_archflags["mips-elf"] 	= "-EB";
		m_archflags["mipsel-elf"] 	= "-EL";
	}

	/////////////////////////////////////////////////////////////////////////////
	// ARM

	else if(arch == "arm-linux-gnueabihf")
	{
		m_archflags[arch] = "";
	}

	else if(arch == "arm-linux-gnueabi")
	{
		m_archflags[arch] = "";
	}

	else if(arch == "arm-none-eabi")
	{
		m_archflags[arch] = "";
	}

	/////////////////////////////////////////////////////////////////////////////
	// INVALID

	else
	{
		LogWarning("Don't know what flags to use for target %s\n", arch.c_str());
	}
}

void GNUToolchain::FindDefaultIncludePaths(vector<string>& paths, string exe, bool cpp, string arch)
{
	//LogDebug("    Finding default include paths\n");

	//We must have valid flags for this arch
	if(!VerifyFlags(arch))
		return;
	string aflags = m_archflags[arch];

	//Ask the compiler what the paths are
	vector<string> lines;
	string cmd = exe + " " + aflags + " -E -Wp,-v ";
	if(cpp)
		cmd += "-x c++ ";
	cmd += "- < /dev/null 2>&1";
	ParseLines(ShellCommand(cmd), lines);

	//Look for the beginning of the search path list
	size_t i = 0;
	for(; i<lines.size(); i++)
	{
		auto line = lines[i];
		if(line.find("starts here") != string::npos)
			break;
	}

	//Get the actual paths
	for(; i<lines.size(); i++)
	{
		auto line = lines[i];
		if(line == "End of search list.")
			break;
		if(line[0] == '#')
			continue;
		paths.push_back(line.substr(1));
	}

	//Debug dump
	//for(auto p : paths)
	//	LogDebug("        %s\n", p.c_str());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Meta-flag processing

/**
	@brief Convert a build flag to the actual executable flag representation
 */
string GNUToolchain::FlagToString(BuildFlag flag)
{
	string s = flag;

	//Optimization flags
	if(s == "optimize/none")
		return "-O0";
	else if(s == "optimize/speed")
		return "-O2";

	//Debug info flags
	else if(s == "debug/gdb")
		return "-ggdb";

	//Language dialect
	else if(s == "dialect/c++11")
		return "--std=c++11";

	//Warning levels
	else if(s == "warning/max")
		return "-Wall -Wextra -Wcast-align -Winit-self -Wmissing-declarations -Wswitch -Wwrite-strings";

	else
	{
		LogWarning("GNUToolchain does not implement flag %s yet\n", s.c_str());
		return "";
	}
}

/**
	@brief
 */
bool GNUToolchain::VerifyFlags(string triplet)
{
	//We must have valid flags for this arch
	if(m_archflags.find(triplet) == m_archflags.end())
	{
		LogError("Don't know how to target %s\n", triplet.c_str());
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual operations

/**
	@brief Scans a source file for dependencies (include files, etc)
 */
bool GNUToolchain::ScanDependencies(
	string exe,
	string triplet,
	string path,
	string root,
	set<BuildFlag> flags,
	const vector<string>& sysdirs,
	set<string>& deps,
	map<string, string>& dephashes)
{
	//Make sure we're good on the flags
	if(!VerifyFlags(triplet))
		return false;

	//Look up some arch-specific stuff
	string aflags = m_archflags[triplet];
	auto apath = m_virtualSystemIncludePath[triplet];

	//Make the full scan command line
	string cmdline = exe + " " + aflags + " -M -MG ";
	for(auto f : flags)
		cmdline += FlagToString(f) + " ";
	cmdline += path;
	LogDebug("Command line: %s\n", cmdline.c_str());

	//Run it
	string makerule = ShellCommand(cmdline);

	//Parse it
	size_t offset = makerule.find(':');
	if(offset == string::npos)
	{
		LogError("GNUToolchain::ScanDependencies - Make rule was not well formed (scan error?)\n");
		return false;
	}
	vector<string> files;
	string tmp;
	for(size_t i=offset + 1; i<makerule.length(); i++)
	{
		bool last_was_slash = (makerule[i-1] == '\\');

		//Skip leading spaces
		char c = makerule[i];
		if( tmp.empty() && isspace(c) )
			continue;

		//If not a space, it's part of a file name
		else if(!isspace(c))
			tmp += c;

		//Space after a slash is part of a file name
		else if( (c == ' ') && last_was_slash)
			tmp += c;

		//Any other space is a delimiter
		else
		{
			if(tmp != "\\")
				files.push_back(tmp);
			tmp = "";
		}
	}
	if(!tmp.empty() && (tmp != "\\") )
		files.push_back(tmp);

	//First entry is always the source file itself, so we can skip that
	//Loop over the other entries and convert them to system/project relative paths
	//LogDebug("Absolute paths:\n");
	for(size_t i=1; i<files.size(); i++)
	{
		//If the path begins with our working copy directory, trim it off and call the rest the relative path
		string f = files[i];
		if(f.find(root) == 0)
		{
			//LogDebug("        local dir\n");
			f = f.substr(root.length() + 1);
		}

		//No go - loop over the system include paths
		else
		{
			string longest_prefix = "";
			for(auto dir : sysdirs)
			{
				if(f.find(dir) != 0)
					continue;

				if(dir.length() > longest_prefix.length())
					longest_prefix = dir;
			}

			//If it's not in the project dir OR a system path, we have a problem.
			//Including random headers by absolute path is not portable!
			if(longest_prefix == "")
			{
				LogError("Path %s could not be resolved to a system or project include directory\n",
					f.c_str());
				return false;
			}

			//Trim off the prefix and go
			//LogDebug("        system dir %s\n", longest_prefix.c_str());
			f = apath + "/" + f.substr(longest_prefix.length() + 1);
		}

		//TODO: Don't even read the file if we already have it in the cache?

		//Add file to cache
		string data = GetFileContents(files[i]);
		string hash = sha256(data);
		if(!g_cache->IsCached(hash))
			g_cache->AddFile(f, hash, hash, data, "");

		//Add virtual path to output dep list
		deps.emplace(f);
		dephashes[f] = hash;
	}

	/*
	LogDebug("    Project-relative dependency paths:\n");
	for(auto f : deps)
		LogDebug("        %s\n", f.c_str());
	*/

	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual compilation

/**
	@brief Compile one or more source files to an object file
 */
bool GNUToolchain::Compile(
	string exe,
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	//If any source has a .o extension, we're linking and not compiling
	for(auto s : sources)
	{
		if(s.find(".o") != string::npos)
			return Link(exe, triplet, sources, fname, flags, outputs, stdout);
	}

	LogDebug("Compile for arch %s\n", triplet.c_str());
	LogIndenter li;

	if(!VerifyFlags(triplet))
		return false;

	//Look up some arch-specific stuff
	string aflags = m_archflags[triplet];
	auto apath = m_virtualSystemIncludePath[triplet];

	//Make the full compile command line
	string cmdline = exe + " " + aflags + " -o " + fname + " ";
	for(auto f : flags)
		cmdline += FlagToString(f) + " ";
	cmdline += "-c ";
	for(auto s : sources)
		cmdline += s + " ";
	LogDebug("Command line: %s\n", cmdline.c_str());

	//Run the compile itself
	if(0 != ShellCommand(cmdline, stdout))
		return false;

	//Get the outputs
	vector<string> files;
	FindFiles(".", files);

	//No filtering needed, everything is toolchain output
	for(auto f : files)
	{
		f = GetBasenameOfFile(f);
		outputs[f] = sha256_file(f);
	}

	//All good if we get here
	return true;
}

bool GNUToolchain::Link(
	string exe,
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& stdout)
{
	LogDebug("Link for arch %s\n", triplet.c_str());
	LogIndenter li;

	if(!VerifyFlags(triplet))
		return false;

	//Look up some arch-specific stuff
	//Link using gcc/g++ instead of ld
	string aflags = m_archflags[triplet];

	//Make the full compile command line
	string cmdline = exe + " " + aflags + " -o " + fname + " ";
	for(auto f : flags)
		cmdline += FlagToString(f) + " ";
	for(auto s : sources)
		cmdline += s + " ";

	LogDebug("Command line: %s\n", cmdline.c_str());

	//Run the compile itself
	if(0 != ShellCommand(cmdline, stdout))
	{
		LogDebug("link failed\n");
		return false;
	}

	//Get the outputs
	vector<string> files;
	FindFiles(".", files);

	//No filtering needed, everything is toolchain output
	for(auto f : files)
	{
		f = GetBasenameOfFile(f);
		outputs[f] = sha256_file(f);
	}

	//All good if we get here
	return true;
}
