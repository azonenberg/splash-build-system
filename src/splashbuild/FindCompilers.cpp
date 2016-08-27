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
void FindXilinxISECompilers();
void FindXilinxVivadoCompilers();

/**
	@brief Search for all C/C++ compilers (of any type) on the current system.
 */
void FindCPPCompilers()
{
	LogDebug("Searching for C/C++ compilers...\n");
	FindGCCCompilers();
	FindClangCompilers();
}

/**
	@brief Search for all linkers (of any type) on the current system.
 */
void FindLinkers()
{
	LogDebug("Searching for linkers...\n");

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
	FindXilinxISECompilers();
	FindXilinxVivadoCompilers();
}

void FindGCCCompilers()
{
	LogDebug("    Searching for GCC compilers...\n");
	
	//Find all directories that normally have executables in them
	vector<string> path;
	ParseSearchPath(path);
	for(auto dir : path)
	{
		//Find all files in this directory that have gcc- in the name.
		//Note that this will often include things like nm, as, etc so we have to then search for numbers at the end to confirm
		vector<string> exes;
		FindFilesBySubstring(dir, "gcc-", exes);
		for(auto exe : exes)
		{			
			//Trim off the directory and see if we have a name of the format [maybe arch triplet-]gcc-%d.%d
			string base = GetBasenameOfFile(exe);
			size_t offset = base.find("gcc");
			
			//If no triplet found ("gcc" at start of string), or no "gcc" found, this is the system default gcc
			//Ignore that, it's probably a symlink to/from an arch specific gcc anyway
			if( (offset == 0) || (offset == string::npos) )
				continue;
			string triplet = base.substr(0, offset-1);
			
			//The text after the triplet should be of the form gcc-major.minor
			string remainder = base.substr(offset);
			int major;
			int minor;
			if(2 != sscanf(remainder.c_str(), "gcc-%4d.%4d", &major, &minor))
				continue;
			
			//Create the toolchain object
			auto gcc = new GNUCToolchain(exe, triplet);
			g_toolchains[gcc->GetHash()] = gcc;
			
			//See if we have a matching G++ for the same triplet and version
			string gxxpath = str_replace("gcc-", "g++-", exe);
			if(DoesFileExist(gxxpath))
			{
				auto gxx = new GNUCPPToolchain(gxxpath, triplet);
				g_toolchains[gxx->GetHash()] = gxx;
			}
		}
	}
}

void FindClangCompilers()
{
	LogDebug("    Searching for Clang compilers...\n");
	LogDebug("        Not implemented yet\n");
}

void FindXilinxISECompilers()
{
	LogDebug("    Searching for Xilinx ISE compilers...\n");
	
	//TODO: add an environment variable, argument, etc to override this if anybody is weird and installed elsewhere
	//Canonicalize the path in case /opt is a symlink to NFS etc
	string xilinxdir = "/opt/Xilinx";
	if(!DoesDirectoryExist(xilinxdir))
		LogDebug("        No %s found, giving up\n", xilinxdir.c_str());
	xilinxdir = CanonicalizePath(xilinxdir);
		
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
	LogDebug("    Searching for Xilinx Vivado compilers...\n");
	
	//TODO: add an environment variable, argument, etc to override this if anybody is weird and installed elsewhere
	//Canonicalize the path in case /opt is a symlink to NFS etc
	string xilinxdir = "/opt/Xilinx/Vivado";
	if(!DoesDirectoryExist(xilinxdir))
		LogDebug("        No %s found, giving up\n", xilinxdir.c_str());
	xilinxdir = CanonicalizePath(xilinxdir);
		
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
