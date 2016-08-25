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
// Accessors

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

void Toolchain::GetSupportedLanguages(vector<string>& langs)
{
	vector<Language> v;
	GetSupportedLanguages(v);
	
	for(auto x : v)
	{
		switch(x)
		{
			case LANG_OBJECT:
				langs.push_back("Object files");
				break;
				
			case LANG_C:
				langs.push_back("C");
				break;
				
			case LANG_CPP:
				langs.push_back("C++");
				break;
				
			case LANG_ASM:
				langs.push_back("Assembly");
				break;
			
			case LANG_VERILOG:
				langs.push_back("Verilog");
				break;
				
			default:
				langs.push_back("invalid");
				break;
		}
	}
}
