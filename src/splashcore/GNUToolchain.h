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

#ifndef GNUToolchain_h
#define GNUToolchain_h

/**
	@brief Common code shared by GNUCToolchain and GNUCPPToolchain
 */
class GNUToolchain
{
public:
	enum GNUType
	{
		GNU_C,
		GNU_CPP,
		GNU_LD
	};

	GNUToolchain(std::string arch, std::string exe, GNUType type);

	void FindDefaultIncludePaths(std::vector<std::string>& paths, std::string exe, bool cpp, std::string arch);

	std::string ParseStringVersion(std::string basepath);

	bool ScanDependencies(
		Toolchain* chain,
		std::string triplet,
		std::string path,
		std::string root,
		std::set<BuildFlag> flags,
		const std::vector<std::string>& sysdirs,
		std::set<std::string>& deps,
		std::map<std::string, std::string>& dephashes,
		std::string& output,
		std::set<std::string>& missingFiles,
		std::set<BuildFlag>& libFlags,
		bool cpp);

	virtual bool Compile(
		Toolchain* chain,
		std::string exe,
		std::string triplet,
		std::set<std::string> sources,
		std::string fname,
		std::set<BuildFlag> flags,
		std::map<std::string, std::string>& outputs,
		std::string& output,
		bool cpp);

	virtual bool Link(
		Toolchain* chain,
		std::string exe,
		std::string triplet,
		std::set<std::string> sources,
		std::string fname,
		std::set<BuildFlag> flags,
		std::map<std::string, std::string>& outputs,
		std::string& output,
		bool cpp);

	bool HasValidArches();

protected:
	std::string FlagToString(BuildFlag flag, std::string triplet);
	bool VerifyFlags(std::string triplet);

	bool FindLibraries(
		Toolchain* chain,
		std::string triplet,
		std::set<BuildFlag> flags,
		std::set<std::string>& foundpaths,
		std::set<std::string>& foundlibNames,
		std::set<BuildFlag>& libflags_in,
		std::string& output);

	/**
		@brief Virtual include path we use for system include files.

		We use this instead of an absolute path because it's portable across systems.
	 */
	std::map<std::string, std::string> m_virtualSystemIncludePath;

	/**
		@brief Remapping of original system include paths to unique identifiers
	 */
	std::map<std::string, std::string> m_searchdirRemap;

	/**
		@brief Flags we use to target each of our supported architectures
	 */
	std::map<std::string, std::string> m_archflags;

	/**
		@brief Map of (architecture, lib name) -> path tuples
	 */
	std::map<std::pair<std::string, std::string>, std::string> m_libpaths;

	/**
		@brief Hashes for a bunch of libc stuff
	 */
	std::string m_internalLibs;

	/**
		@brief Profiling instrumentation
	 */
	std::map<std::string, int> m_timesScanned;
};

#endif

