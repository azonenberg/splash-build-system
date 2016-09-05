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
	char group[32];
	char name[32];
	if(2 != sscanf(flag.c_str(), "%31[^/]/%31s", group, name))
	{
		LogParseError("Flag \"%s\" is malformed (expected \"category/flag\" form)\n", flag.c_str());
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
	else
		LogParseError("Unknown flag group \"%s\"\n", group);
}

BuildFlag::~BuildFlag()
{
	
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

/**
	@brief Checks if this flag is used at a particular step in the build process
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
