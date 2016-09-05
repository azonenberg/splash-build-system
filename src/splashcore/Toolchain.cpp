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

Toolchain::Toolchain(string basepath, ToolchainType type)
	: m_basepath(basepath)
	, m_type(type)
{
}

Toolchain::~Toolchain()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug helpers

void Toolchain::DebugPrint()
{
	vector<string> langs;
	GetSupportedLanguages(langs);

	LogVerbose("Toolchain %s:\n", GetHash().c_str());
	LogVerbose("    Type:        %s\n", GetToolchainType().c_str());
	if(GetPatchVersion() != 0)
		LogVerbose("    Version:     %d.%d.%d\n", GetMajorVersion(), GetMinorVersion(), GetPatchVersion());
	else
		LogVerbose("    Version:     %d.%d\n", GetMajorVersion(), GetMinorVersion());
	LogVerbose("    String ver:  %s\n", GetVersionString().c_str());
	LogVerbose("    Path:        %s\n", GetBasePath().c_str());
	LogVerbose("    Source languages:\n");
	for(auto x : langs)
		LogVerbose("        %s\n", x.c_str());
	LogVerbose("    Target triplets:\n");
	for(auto x : m_triplets)
		LogVerbose("        %s\n", x.c_str());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

bool Toolchain::IsArchitectureSupported(string arch)
{
	for(auto x : m_triplets)
	{
		if(x == arch)
			return true;
	}
	return false;
}

bool Toolchain::IsLanguageSupported(Language lang)
{
	for(auto x : m_langs)
	{
		if(x == lang)
			return true;
	}

	return false;
}

string Toolchain::GetToolchainType()
{
	switch(m_type)
	{
		case TOOLCHAIN_GNU:
			return "GNU";

		case TOOLCHAIN_CLANG:
			return "Clang";

		case TOOLCHAIN_ISE:
			return "ISE";

		case TOOLCHAIN_VIVADO:
			return "Vivado";

		default:
			return "invalid";
	}
}

string Toolchain::LangToString(Language lang)
{
	switch(lang)
	{
		case LANG_OBJECT:
			return "Object files";

		case LANG_C:
			return "C";

		case LANG_CPP:
			return "C++";

		case LANG_ASM:
			return "Assembly";

		case LANG_VERILOG:
			return "Verilog";

		default:
			return "invalid";
	}
}

void Toolchain::GetSupportedLanguages(vector<string>& langs)
{
	for(auto x : m_langs)
		langs.push_back(LangToString(x));
}
