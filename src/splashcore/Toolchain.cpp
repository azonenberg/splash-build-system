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
	set<string> langs;
	GetSupportedLanguages(langs);

	LogVerbose("Toolchain %s:\n", GetHash().c_str());
	LogIndenter li;
	LogVerbose("Type:        %s\n", GetToolchainType().c_str());
	if(GetPatchVersion() != 0)
		LogVerbose("Version:     %d.%d.%d\n", GetMajorVersion(), GetMinorVersion(), GetPatchVersion());
	else
		LogVerbose("Version:     %d.%d\n", GetMajorVersion(), GetMinorVersion());
	LogVerbose("String ver:  %s\n", GetVersionString().c_str());
	LogVerbose("Path:        %s\n", GetBasePath().c_str());
	LogVerbose("Source languages:\n");
	for(auto x : langs)
		LogVerbose("    %s\n", x.c_str());

	LogVerbose("Target triplets:\n");
	LogIndenter li2;
	for(auto x : m_triplets)
		LogVerbose("%s\n", x.c_str());
}

bool Toolchain::ScanDependencies(
	string triplet,
	string path,
	string root,
	set<BuildFlag> flags,
	set<string>& deps,
	map<string, string>& dephashes,
	string& output,
	set<string>& missingFiles,
	set<BuildFlag>& libFlags)
{
	//Check if we already ran this exact scan before
	string scanhash = m_depCache.GetHash(path, triplet, flags);
	if(m_depCache.IsCached(scanhash))
	{
		//LogDebug("[Toolchain::ScanDependencies] Results for %s were cached\n", path.c_str());
		auto& results = m_depCache.GetCachedDependencies(scanhash);

		//Copy the outputs
		output = results.m_stdout;
		deps = results.m_deps;
		dephashes = results.m_filedeps;
		libFlags = results.m_libflags;

		//never any missing files in the cache
		missingFiles.clear();

		//Done
		return results.m_ok;
	}

	bool ok = ScanDependenciesUncached(triplet, path, root, flags, deps, dephashes, output, missingFiles, libFlags);

	//Put it in the cache, regardless of success status.
	//Do NOT cache results if we have missing files since the scan is incomplete.
	if(missingFiles.empty())
	{
		CachedDependencies cdeps;
		cdeps.m_ok = ok;
		cdeps.m_stdout = output;
		cdeps.m_deps = deps;
		cdeps.m_filedeps = dephashes;
		cdeps.m_libflags = libFlags;
		m_depCache.AddEntry(scanhash, cdeps);
	}

	//and we're done
	return ok;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

/**
	@brief Compare our version and type to another toolchain

	@return 1 if we're greater
			0 if equal
			-1 if other is greater
 */
int Toolchain::CompareVersionAndType(Toolchain* rhs)
{
	if(m_type > rhs->m_type)
		return 1;
	else if(m_type < rhs->m_type)
		return -1;

	return CompareVersion(rhs);
}

/**
	@brief Compare our version to another toolchain

	@return 1 if we're greater
			0 if equal
			-1 if other is greater
 */
int Toolchain::CompareVersion(Toolchain* rhs)
{
	if(m_majorVersion > rhs->m_majorVersion)
		return 1;
	else if(m_majorVersion < rhs->m_majorVersion)
		return -1;

	if(m_minorVersion > rhs->m_minorVersion)
		return 1;
	else if(m_minorVersion < rhs->m_minorVersion)
		return -1;

	if(m_patchVersion > rhs->m_patchVersion)
		return 1;
	else if(m_patchVersion < rhs->m_patchVersion)
		return -1;

	return 0;
}

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
			return "Object";

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

void Toolchain::GetSupportedLanguages(set<string>& langs)
{
	for(auto x : m_langs)
		langs.emplace(LangToString(x));
}

/**
	@brief Get the full list of compiler names we implement
 */
void Toolchain::GetCompilerNames(set<string>& names)
{
	//Toolchain type
	string type = MakeStringLowercase(GetToolchainType());

	//Set of languages we support
	set<string> langs;
	for(auto x : m_langs)
		langs.emplace(MakeStringLowercase(LangToString(x)));

	//Set of version numbers we implement
	unordered_set<string> versions;
	char tmp[128];
	snprintf(tmp, sizeof(tmp), "%d", m_majorVersion);
	versions.emplace(tmp);
	snprintf(tmp, sizeof(tmp), "%d.%d", m_majorVersion, m_minorVersion);
	versions.emplace(tmp);
	snprintf(tmp, sizeof(tmp), "%d.%d.%d", m_majorVersion, m_minorVersion, m_patchVersion);
	versions.emplace(tmp);

	//Generate the full set
	for(auto l : langs)
	{
		for(auto v : versions)
			names.emplace(string(l + "/" + type + "/" + v));
		names.emplace(string(l + "/" + type));
	}
}
