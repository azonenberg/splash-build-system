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

bool XilinxISEToolchain::Build(
	string /*triplet*/,
	set<string> /*sources*/,
	string /*fname*/,
	set<BuildFlag> /*flags*/,
	map<string, string>& /*outputs*/,
	string& /*stdout*/)
{
	//

	LogError("XilinxISEToolchain::Build() not implemented\n");
	return false;
}
