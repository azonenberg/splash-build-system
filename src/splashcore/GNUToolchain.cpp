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
	//LogDebug("    Finding default include paths for %s (c++=%d)\n", arch.c_str(), cpp);

	//We must have valid flags for this arch
	if(!VerifyFlags(arch))
		return;
	string aflags = m_archflags[arch];

	//Ask the compiler what the paths are
	vector<string> lines;
	string cmd = exe + " " + aflags + " -c --verbose ";
	if(cpp)
		cmd += "-x c++ ";
	else
		cmd += "-x c ";
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

		//Some compilers end paths with a dot. Trim it off
		if(line[line.length() - 1] == '.')
			paths.push_back(line.substr(1, line.length() - 2));

		else
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

	//Output file formats
	else if(s == "output/shared")
		return "-shared";
	else if(s == "output/reloc")
		return "-fPIC";

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
	map<string, string>& dephashes,
	string& output,
	set<string>& missingFiles,
	bool cpp)
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
	if(cpp)
		cmdline += "-x c++ ";
	else
		cmdline += "-x c ";
	cmdline += path;
	cmdline += " 2>&1";
	//LogDebug("Dep command line: %s\n", cmdline.c_str());

	//Run it
	string makerule;
	if(0 != ShellCommand(cmdline, makerule))
	{
		output = makerule;
		return false;
	}

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
	LogIndenter li;
	for(size_t i=1; i<files.size(); i++)
	{
		string f = files[i];
		//LogDebug("%s\n", f.c_str());
		LogIndenter li;

		//If the path begins with our working copy directory, trim it off and call the rest the relative path
		if(f.find(root) == 0)
		{
			//LogDebug("        local dir\n");
			f = f.substr(root.length() + 1);
		}

		//No go - loop over the system include paths
		else
		{
			//If it's located in a system include directory, that's a hit
			string longest_prefix = "";
			for(auto dir : sysdirs)
			{
				if(f.find(dir) != 0)
					continue;

				//if(dir.length() > longest_prefix.length())
				//	longest_prefix = dir;

				//Match the FIRST directory
				longest_prefix = dir;
				break;
			}

			//It's an absolute path to a standard system include directory
			if(longest_prefix != "")
			{
				//Trim off the prefix and go
				string old_f = f;
				f = apath + "/" + f.substr(longest_prefix.length() + 1);
			}

			//If it's an absolute path but NOT in a system include dir, fail.
			//Including random headers by absolute path is not portable!
			else if(f[0] == '/')
			{
				char tmp[1024];
				snprintf(tmp, sizeof(tmp),
					"Absolute path %s could not be resolved to a system include directory\n",
					f.c_str());
				output += tmp;
				return false;
			}

			//Relative path - we don't have it locally
			//Ask the server for it (by best-guess filename)
			//TODO: Canonicalize this?
			else if(longest_prefix == "")
			{
				//LogDebug("Unable to resolve include %s\n", f.c_str());
				missingFiles.emplace(str_replace(root + "/", "", GetDirOfFile(path)) + "/" + f);
				continue;
			}
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
	string& output,
	bool cpp)
{
	//If any source has a .o extension, we're linking and not compiling
	for(auto s : sources)
	{
		if(s.find(".o") != string::npos)
			return Link(exe, triplet, sources, fname, flags, outputs, output, cpp);
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
	cmdline += "-nostdinc ";					//make sure we only include files the server provided
	cmdline += "-nostdinc++ ";
	if(cpp)
		cmdline += "-x c++ ";
	else
		cmdline += "-x c ";
	for(auto f : flags)							//special flags
		cmdline += FlagToString(f) + " ";
	cmdline += string("-I") + apath + "/ ";		//include the virtual system path
	cmdline += "-c ";
	for(auto s : sources)
		cmdline += s + " ";
	LogDebug("Compile command line: %s\n", cmdline.c_str());

	//Run the compile itself
	if(0 != ShellCommand(cmdline, output))
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
	string& output,
	bool /*cpp*/)
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
	if(0 != ShellCommand(cmdline, output))
	{
		LogDebug("link failed\n");
		//LogError("%s\n", output.c_str());
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
