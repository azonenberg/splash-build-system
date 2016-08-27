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

#ifndef Toolchain_h
#define Toolchain_h

/**
	@brief A toolchain for compiling and/or linking some language(s)
 */
class Toolchain
{
public:
	/**
		@brief Type of compiler

		Allows build server to report installed compilers to splashctl
	 */
	enum ToolchainType
	{
		TOOLCHAIN_GNU,		//GNU C/C++
		TOOLCHAIN_CLANG,	//Clang
		TOOLCHAIN_ISE,		//Xilinx ISE
		TOOLCHAIN_VIVADO,	//Xilinx Vivado

		TOOLCHAIN_LAST		//placeholder
	};
	
	Toolchain(std::string basepath, ToolchainType type);
	virtual ~Toolchain();
	
	/**
		@brief Print all of our configuration for debugging
	 */
	void DebugPrint();

	//IDs for all supported languages in Splash (add more here as necessary)
	enum Language
	{
		//Non-languages
		LANG_OBJECT,			//object files only (used for linkers)

		//High-level languages
		LANG_C,					//C source
		LANG_CPP,				//C++ source

		//Assembly languages
		LANG_ASM,				//Assembly language (no syntax specified)
		//TODO: dialects like ATT/Intel

		//HDLs
		LANG_VERILOG,			//Verilog source

		LANG_MAX				//end marker
	};
	
	/**
		@brief Get the list of languages that we can compile.
	 */
	const std::vector<Language>& GetSupportedLanguages()
	{ return m_langs; }
	
	/**
		@brief Get the list of languages that we can compile, as strings (for debug)
	 */
	void GetSupportedLanguages(std::vector<std::string>& langs);
	
	/**
		@brief Get the type of the toolchain as a string (one word)
	 */
	std::string GetToolchainType();
	
	ToolchainType GetType()
	{ return m_type; }

	/**
		@brief Get the list of architecture triplets that we can target.
	 */
	void GetTargetTriplets(std::vector<std::string>& triplets)
	{ triplets = m_triplets; }

	/**
		@brief Get a hash that uniquely identifies this toolchain (including target arch and patch level)
	 */
	std::string GetHash()
	{ return m_hash; }
	
	/**
		@brief Get major toolchain version
	 */
	int GetMajorVersion()
	{ return m_majorVersion; }
	
	/**
		@brief Get minor toolchain version
	 */
	int GetMinorVersion()
	{ return m_minorVersion; }
	
	/**
		@brief Get patch toolchain version
	 */
	int GetPatchVersion()
	{ return m_patchVersion; }
	
	/**
		@brief Get base path of toolchain (debug only, don't expect this to be an exe or dir or anything useful)
	 */
	std::string GetBasePath()
	{ return m_basepath; }
	
	std::string GetVersionString()
	{ return m_stringVersion; }
	
protected:

	/**
		@brief Base path for the toolchain.

		This may be either the compiler executable for something like gcc, or
		the base install directory for e.g. an FPGA tool suite.
	 */
	std::string m_basepath;
	
	/**
		@brief Type of the toolchain (used when a dev requests a specific toolchain by name)
	 */
	ToolchainType m_type;
	
	/**
		@brief A hash that uniquely identifies this toolchain
		
		Must be set in the constructor.
	 */
	std::string m_hash;
	
	/**
		@brief List of architecture triplets we support
		
		Must be set in the constructor.
	 */
	std::vector<std::string> m_triplets;
	
	/**
		@brief List of languages we support
		
		Must be set in the constructor.
	 */
	std::vector<Language> m_langs;
	
	/**
		@brief Toolchain major version
	 */
	int m_majorVersion;
	
	/**
		@brief Toolchain minor version
	 */
	int m_minorVersion;
	
	/**
		@brief Toolchain patch version
	 */
	int m_patchVersion;
	
	/**
		@brief Toolchain string version. This may include info like distro patches etc
	 */
	std::string m_stringVersion;
};

#endif
