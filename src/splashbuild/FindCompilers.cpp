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

#include "splashbuild.h"

using namespace std;

void FindGCCCompilers();
void FindClangCompilers();
void FindYosysCompilers();
void FindXilinxISECompilers();
void FindXilinxVivadoCompilers();

string GetXilinxDirectory();

/**
	@brief Search for all C/C++ compilers (of any type) on the current system.
 */
void FindCPPCompilers()
{
	LogDebug("Searching for C/C++ compilers...\n");
	LogIndenter li;
	FindGCCCompilers();
	FindClangCompilers();
}

/**
	@brief Search for all linkers (of any type) on the current system.
 */
void FindLinkers()
{
	LogDebug("Searching for linkers...\n");
	LogIndenter li;

	//Find all directories that normally have executables in them
	vector<string> path;
	ParseSearchPath(path);
	for(auto dir : path)
	{
		//Find all files in this directory that have gcc- in the name.
		//Note that this will often include things like nm, as, etc so we have to then search for numbers at the end to confirm
		vector<string> exes;
		FindFilesBySubstring(dir, "-ld", exes);
		for(auto exe : exes)
		{
			//Trim off the directory and see if we have a name of the format [arch triplet]-ld
			string base = GetBasenameOfFile(exe);
			size_t offset = base.find("-ld");

			//Sanity check that we have a triplet
			//If we don't end in "-ld" we're an internal file, ignore it
			if( (offset != (base.length() - 3)) || (offset == 0) || (offset == string::npos))
				continue;
			string triplet = base.substr(0, offset);

			//See if it's a GNU linker
			string ver = ShellCommand(exe + " --version | head -n 1 | grep GNU");
			if(ver == "")
			{
				LogWarning("Ignoring linker %s because it's not a GNU linker\n", exe.c_str());
				continue;
			}

			//Create the linker
			auto ld = new GNULinkerToolchain(exe, triplet);
			g_toolchains[ld->GetHash()] = ld;
		}
	}
}

/**
	@brief Search for all FPGA compilers (of any type) on the current system.
 */
void FindFPGACompilers()
{
	LogDebug("Searching for FPGA compilers...\n");
	LogIndenter li;

	FindXilinxISECompilers();
	FindXilinxVivadoCompilers();
	FindYosysCompilers();
}

void FindGCCCompilers()
{
	LogDebug("Searching for GCC compilers...\n");
	LogIndenter li;

	//Find all directories that normally have executables in them
	vector<string> path;
	ParseSearchPath(path);
	for(auto dir : path)
	{
		//Find all files in this directory that have gcc- in the name.
		vector<string> exes;
		FindFilesBySubstring(dir, "-gcc", exes);
		for(auto exe : exes)
		{
			//Trim off the directory and see if we have a name of the format [maybe arch triplet]-gcc[maybe more stuff]
			string base = GetBasenameOfFile(exe);
			size_t offset = base.find("-gcc");

			//If we have any binutils names in the filename we're not a compiler
			if( (base.find("gcc-ar") != string::npos) ||
				(base.find("gcc-nm") != string::npos) ||
				(base.find("gcc-ranlib") != string::npos) )
			{
				continue;
			}

			//If no triplet found ("gcc" at start of string), or no "gcc" found, this is the system default gcc
			//Ignore that, it's probably a symlink to/from an arch specific gcc anyway
			if( (offset == 0) || (offset == string::npos) )
				continue;
			string triplet = base.substr(0, offset);

			//Skip triplet "afl", this is for fuzzing (not yet supported in splash)
			if(triplet == "afl")
				continue;

			//Skip triplets "c89" and "c99", these aren't real architectures
			if( (triplet == "c89") || (triplet == "c99") )
				continue;

			//Don't require a version number since Red Hat et al don't always have one

			//Red Hat triplets are weird. Normalize to standard GNU triplets
			if(triplet.find("redhat-linux") != string::npos)
				triplet = str_replace("redhat-linux", "linux-gnu", triplet);

			//Create the toolchain object
			auto gcc = new GNUCToolchain(exe, triplet);
			if(gcc->HasValidArches())
				g_toolchains[gcc->GetHash()] = gcc;
			else
			{
				delete gcc;
				LogWarning("Toolchain \"%s\" has no valid target architectures, skipping it\n", exe.c_str());
				continue;
			}

			//See if we have a matching G++ for the same triplet and version
			string gxxpath = str_replace("gcc", "g++", exe);
			if(DoesFileExist(gxxpath))
			{
				auto gxx = new GNUCPPToolchain(gxxpath, triplet);
				if(gxx->HasValidArches())
					g_toolchains[gxx->GetHash()] = gxx;
				else
				{
					delete gxx;
					LogWarning("Toolchain \"%s\" has no valid target architectures, skipping it\n", gxxpath.c_str());
					continue;
				}
			}
		}
	}
}

void FindClangCompilers()
{
	LogDebug("Searching for Clang compilers...\n");
	LogIndenter li;

	LogDebug("Not implemented yet\n");
}

string GetXilinxDirectory()
{
	//Look in /opt/Xilinx but override with environment variable if present
	string xilinxdir = "/opt/Xilinx";
	char* evar = getenv("XILINX");
	if(evar != NULL)
		xilinxdir = evar;

	if(!DoesDirectoryExist(xilinxdir))
	{
		LogDebug("No %s found, giving up\n", xilinxdir.c_str());
		return "";
	}

	//Canonicalize the path in case /opt is a symlink to NFS etc
	return CanonicalizePath(xilinxdir);
}

void FindXilinxISECompilers()
{
	LogDebug("Searching for Xilinx ISE compilers...\n");
	LogIndenter li;

	string xilinxdir = GetXilinxDirectory();
	if(xilinxdir == "")
		return;

	//Search for all subdirectories and see if any of them look like ISE
	vector<string> dirs;
	FindSubdirs(xilinxdir, dirs);
	for(auto dir : dirs)
	{
		//Any ##.## format directory is probably an ISE installation
		int major;
		int minor;
		string format = xilinxdir + "/%d.%d";
		if(2 != sscanf(dir.c_str(), format.c_str(), &major, &minor))
			continue;

		//If the version is not 14.7, ignore it (old ISE is deader than dead)
		if( (major != 14) || (minor != 7))
		{
			LogWarning("Ignoring old ISE version %d.%d (only 14.7 is supported for now)\n", major, minor);
			continue;
		}

		//To make sure, look for XST
		string expected_xst_path = dir + "/ISE_DS/ISE/bin/lin64/xst";
		if(!DoesFileExist(expected_xst_path))
			continue;

		auto ise = new XilinxISEToolchain(dir, major, minor);
		g_toolchains[ise->GetHash()] = ise;
	}
}

void FindXilinxVivadoCompilers()
{
	LogDebug("Searching for Xilinx Vivado compilers...\n");
	LogIndenter li;

	string xilinxdir = GetXilinxDirectory();
	if(xilinxdir == "")
		return;

	//Search for all subdirectories and see if any of them look like Vivado
	vector<string> dirs;
	FindSubdirs(xilinxdir, dirs);
	for(auto dir : dirs)
	{
		//Any ##.## format directory is probably a Vivado installation
		int major;
		int minor;
		string format = xilinxdir + "/%d.%d";
		if(2 != sscanf(dir.c_str(), format.c_str(), &major, &minor))
			continue;

		//To make sure, look for the Vivado executable
		string expected_vivado_path = dir + "/bin/vivado";
		if(!DoesFileExist(expected_vivado_path))
			continue;

		auto vdo = new XilinxVivadoToolchain(dir, major, minor);
		g_toolchains[vdo->GetHash()] = vdo;
	}
}

void FindYosysCompilers()
{
	LogDebug("Searching for Yosys compilers...\n");
	LogIndenter li;

	//Find all directories that normally have executables in them
	vector<string> path;
	ParseSearchPath(path);
	for(auto dir : path)
	{
		string basepath = dir + "/yosys";
		if(DoesFileExist(basepath))
		{
			auto chain = new YosysToolchain(basepath);
			g_toolchains[chain->GetHash()] = chain;
			return;
		}
	}

	LogDebug("Could not find Yosys in search path\n");
}
