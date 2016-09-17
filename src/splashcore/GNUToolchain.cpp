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

void GNUToolchain::FindDefaultIncludePaths(vector<string>& paths, string exe, bool cpp)
{
	//LogDebug("    Finding default include paths\n");

	//Ask the compiler what the paths are
	vector<string> lines;
	string cmd = exe + " -E -Wp,-v ";
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual operations

/**
	@brief Scans a source file for dependencies (include files, etc)


 */
bool GNUToolchain::ScanDependencies(
	string exe,
	string path,
	string root,
	set<BuildFlag> flags,
	const vector<string>& sysdirs,
	set<string>& deps,
	set<string>& missing)
{
	//Make the full scan command line
	string cmdline = exe + " -M -MG ";
	for(auto f : flags)
		cmdline += FlagToString(f) + " ";
	cmdline += path;
	//LogDebug("Command line: %s\n", cmdline.c_str());

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
	for(size_t i=/*1*/0; i<files.size(); i++)
	{
		//If the path begins with our working copy directory, trim it off and call the rest the relative path
		string f = files[i];
		LogDebug("    %s\n", f.c_str());
		if(f.find(root) == 0)
		{
			//LogDebug("        local dir\n");
			f = f.substr(root.length() + 1);
			deps.emplace(f);
			continue;
		}

		//No go - loop over the system include paths
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
		f = string("__sysinclude__/") + f.substr(longest_prefix.length() + 1);
		deps.emplace(f);
	}

	LogDebug("Relative paths:\n");
	for(auto f : deps)
		LogDebug("    %s\n", f.c_str());

	//TODO: Detect which files we do not have in cache and report them as missing

	//TODO: decide if all is good
	return true;
}
