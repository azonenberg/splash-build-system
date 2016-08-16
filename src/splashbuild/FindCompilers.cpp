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
}

void FindClangCompilers()
{
	LogDebug("    Searching for Clang compilers...\n");
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
		int major_version;
		int minor_version;
		string format = xilinxdir + "/%d.%d";
		if(2 != sscanf(dir.c_str(), format.c_str(), &major_version, &minor_version))
			continue;
			
		//To make sure, look for XST
		string expected_xst_path = dir + "/ISE_DS/ISE/bin/lin64/xst";
		if(!DoesFileExist(expected_xst_path))
			continue;
		
		//TODO: save this somewhere
		LogNotice("    Found ISE %d.%d at %s\n", major_version, minor_version, dir.c_str());
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
		int major_version;
		int minor_version;
		string format = xilinxdir + "/%d.%d";
		if(2 != sscanf(dir.c_str(), format.c_str(), &major_version, &minor_version))
			continue;
			
		//To make sure, look for the Vivado executable
		string expected_vivado_path = dir + "/bin/vivado";
		if(!DoesFileExist(expected_vivado_path))
			continue;
		
		//TODO: save this somewhere
		LogNotice("    Found Vivado %d.%d at %s\n", major_version, minor_version, dir.c_str());
	}
}