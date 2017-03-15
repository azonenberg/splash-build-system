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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

BuildFlag::BuildFlag(string flag)
	: m_rawflag(flag)
{
	//If the flag is "global" then no further parsing needed
	if(flag == "global")
	{
		m_usage = NO_TIME;
		m_type = TYPE_META;
		m_flag = "global";
		return;
	}

	//Try to get the info out of it
	char group[32] = "";
	char name[64] = "";
	char arg[64] = "";
	if(3 == sscanf(flag.c_str(), "%31[^/]/%31[^/]/%63s", group, name, arg))
		m_arg = arg;
	else if(2 != sscanf(flag.c_str(), "%31[^/]/%63s", group, name))
	{
		LogParseError(
			"Flag \"%s\" is malformed (expected \"category/flag\" or \"category/flag/arg\" form)\n", flag.c_str());
		return;
	}

	//Read the flag name itself
	m_flag = name;

	//Look up the type
	string sgroup(group);
	if(sgroup == "warning")
		LoadWarningFlag();
	else if(sgroup == "error")
		LoadErrorFlag();
	else if(sgroup == "optimize")
		LoadOptimizeFlag();
	else if(sgroup == "debug")
		LoadDebugFlag();
	else if(sgroup == "analysis")
		LoadAnalysisFlag();
	else if(sgroup == "dialect")
		LoadDialectFlag();
	else if(sgroup == "output")
		LoadOutputFlag();
	else if(sgroup == "library")
		LoadLibraryFlag();
	else if(sgroup == "define")
		LoadDefineFlag();
	else if(sgroup == "hardware")
		LoadHardwareFlag();
	else
		LogParseError("Unknown flag group \"%s\"\n", group);
}

BuildFlag::~BuildFlag()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

/**
	@brief Checks if this flag is used at a particular step in the build process.
 */
bool BuildFlag::IsUsedAt(FlagUsage t)
{
	return (m_usage & t) ? true : false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialization

void BuildFlag::LoadWarningFlag()
{
	m_type = TYPE_WARNING;

	//warning/max: request max available warning level
	if(m_flag == "max")
		m_usage = COMPILE_TIME | LINK_TIME | SYNTHESIS_TIME | MAP_TIME | PAR_TIME | IMAGE_TIME;

	else
		LogParseError("Flag \"warning/%s\" is unknown\n", m_flag.c_str());
}

void BuildFlag::LoadErrorFlag()
{
	m_type = TYPE_ERROR;

	LogParseError("Flag \"error/%s\" is unknown\n", m_flag.c_str());
}

void BuildFlag::LoadOptimizeFlag()
{
	m_type = TYPE_OPTIMIZE;

	//optimize/none: request no optimizations
	if(m_flag == "none")
		m_usage = COMPILE_TIME | LINK_TIME | SYNTHESIS_TIME | MAP_TIME | PAR_TIME;

	//optimize/speed: generic speed optimizations
	else if(m_flag == "speed")
		m_usage = COMPILE_TIME | LINK_TIME | SYNTHESIS_TIME | MAP_TIME | PAR_TIME;

	//optimize/hierarchy: preserve hierarchy through implementation
	else if(m_flag == "hierarchy")
	{
		m_usage = SYNTHESIS_TIME;

		if( (m_arg != "flatten") && (m_arg != "keep") && (m_arg != "synth_only") )
			LogParseError("Optimize/hierarchy argument \"%s\" is invalid\n", m_arg.c_str());
	}

	else
		LogParseError("Flag \"optimize/%s\" is unknown\n", m_flag.c_str());
}

void BuildFlag::LoadDebugFlag()
{
	m_type = TYPE_DEBUG;

	//debug/gdb: generate gdb debug information
	if(m_flag == "gdb")
		m_usage = COMPILE_TIME | LINK_TIME;

	else
		LogParseError("Flag \"debug/%s\" is unknown\n", m_flag.c_str());
}

void BuildFlag::LoadAnalysisFlag()
{
	m_type = TYPE_ANALYSIS;

	if(m_flag == "sanitize")
	{
		m_usage = COMPILE_TIME;

		if( (m_arg != "address") )
			LogParseError("analysis/sanitize argument \"%s\" is invalid\n", m_arg.c_str());
	}

	else
		LogParseError("Flag \"analysis/%s\" is unknown\n", m_flag.c_str());
}

void BuildFlag::LoadDialectFlag()
{
	m_type = TYPE_DIALECT;

	//dialect/c++11: use C++ 11
	if(m_flag == "c++11")
		m_usage = COMPILE_TIME;

	else
		LogParseError("Flag \"dialect/%s\" is unknown\n", m_flag.c_str());
}

void BuildFlag::LoadLibraryFlag()
{
	m_type = TYPE_LIBRARY;
	m_usage = LINK_TIME;

	//library/optional: link to this library but OK if not found
	//Compile time so we can do -DHAVE_FOO
	if(m_flag == "optional")
		m_usage = LINK_TIME | COMPILE_TIME;

	//library/required: link to this library, fail if not found
	//Compile time so we can do -DHAVE_FOO
	else if(m_flag == "required")
		m_usage = LINK_TIME | COMPILE_TIME;

	//library/target: link to this library (which is a target)
	//Link time only, we always have the library in the dependency list
	else if(m_flag == "target")
		m_usage = LINK_TIME;

	//library/__incdir: include directory (MUST be project-relative path)
	else if(m_flag == "__incdir")
		m_usage = SYNTHESIS_TIME | COMPILE_TIME;

	else
		LogParseError("Flag \"library/%s\" is unknown\n", m_flag.c_str());
}

void BuildFlag::LoadOutputFlag()
{
	m_type = TYPE_OUTPUT;

	//output/shared: produce a shared library
	if(m_flag == "shared")
		m_usage = COMPILE_TIME | LINK_TIME;

	//output/reloc: produce relocatable binaries
	else if(m_flag == "reloc")
		m_usage = COMPILE_TIME | LINK_TIME;

	//output/unused: specify behavior of unused FPGA I/Os
	else if(m_flag == "unused")
	{
		m_usage = IMAGE_TIME;

		if( (m_arg != "down") && (m_arg != "up") && (m_arg != "none") && (m_arg != "float") )
			LogParseError("output/unused argument \"%s\" must be down, up, or float\n", m_arg.c_str());
	}

	//output/pull: specify drive strength for pullups on unused FPGA I/Os
	else if(m_flag == "pull")
	{
		m_usage = IMAGE_TIME;

		if( (m_arg != "10k") && (m_arg != "100k") && (m_arg != "1M") )
			LogParseError("output/pull argument \"%s\" must be 10k, 100k, or 1M\n", m_arg.c_str());
	}

	else
		LogParseError("Flag \"output/%s\" is unknown\n", m_flag.c_str());
}

void BuildFlag::LoadDefineFlag()
{
	m_type = TYPE_DEFINE;
	m_usage = COMPILE_TIME | SYNTHESIS_TIME;

	//define/foo or define/foo/value
	if(m_arg == "")
		m_arg = "1";
}

void BuildFlag::LoadHardwareFlag()
{
	m_type = TYPE_HARDWARE;
	m_usage = ALL_TIME;

	//hardware/speed: specify device speed grade
	if(m_flag == "speed")
	{
		if(m_arg == "")
			LogParseError("hardware/speed requires an argument\n");
	}

	//hardware/package: specify device package
	else if(m_flag == "package")
	{
		if(m_arg == "")
			LogParseError("hardware/package requires an argument\n");
	}

	else
		LogParseError("Flag \"hardware/%s\" is unknown\n", m_flag.c_str());
}
