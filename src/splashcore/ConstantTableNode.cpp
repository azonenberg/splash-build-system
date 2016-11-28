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

ConstantTableNode::ConstantTableNode(
	BuildGraph* graph,
	string fname,
	string name,
	const YAML::Node& node,
	string table_fname,
	string generator,
	string yaml_hash)
	: BuildGraphNode(graph, BuildFlag::NO_TIME, fname, "")
{
	//Add dependency for the source file
	m_dependencies.emplace(table_fname);

	//Read the base and enum width
	int base = 10;
	if(node["base"])
	{
		string b = node["base"].as<string>();
		if(b == "hex")
			base = 16;
		else if(b == "dec")
			base = 10;
		else if(b == "bin")
			base = 2;
		else
		{
			LogError("Invalid constant table");
			return;
		}
	}
	int width = 32;
	if(node["width"])
		width = node["width"].as<int>();

	//Calculate our hash
	m_hash = sha256(yaml_hash + "!" + generator + "!" + fname);

	//Read the values
	if(!node["values"])
	{
		LogError("Enum \"%s\" has no values", fname.c_str());
		return;
	}
	map<string, uint64_t> values;
	for(auto it : node["values"])
	{
		string id = it.first.as<string>();

		//If the name is already used, complain
		if(values.find(id) != values.end())
		{
			SetInvalidInput(
				string("ERROR: Attempted redeclaration of name ") + id + " in constant table "
				+ table_fname + "\n");
			return;
		}

		values[id] = strtoul(it.second.as<string>().c_str(), NULL, base);
	}

	//Actually make the table
	GenerateConstants(fname, name, values, generator, width);
}

void ConstantTableNode::DoFinalize()
{
	//nothing to do, all the work is handled in the ctor
}

ConstantTableNode::~ConstantTableNode()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Table generation

void ConstantTableNode::GenerateConstants(
	string fname,
	string name,
	map<string, uint64_t>& table,
	string generator,
	int width)
{
	string code;

	//Create the contents of the file
	if(generator == "c/enum")
		code = GenerateConstantsCEnum(name, table, width);
	else if(generator == "c/define")
		code = GenerateConstantsCDefine(name, table, width);
	else if(generator == "verilog/define")
		code = GenerateConstantsVerilogDefine(name, table, width);
	else if(generator == "verilog/localparam")
		code = GenerateConstantsVerilogLocalparam(name, table, width);
	else
	{
		LogError("Invalid generator %s\n", generator.c_str());
		return;
	}

	//LogDebug("CONSTANT TABLE\n%s\n", code.c_str());

	//Add it to the working copy
	//Don't worry about dirty scripts
	g_cache->AddFile(GetBasenameOfFile(fname), m_hash, sha256(code), code, "");
	set<string> ignored;
	m_graph->GetWorkingCopy()->UpdateFile(fname, m_hash, false, false, ignored);
}

string ConstantTableNode::GenerateConstantsCDefine(string name, map<string, uint64_t>& table, int /*width*/)
{
	//Body
	string retval;
	char val[32];
	for(auto it : table)
	{
		snprintf(val, sizeof(val), "%lx", it.second);
		retval += string("    #define ") + it.first + " 0x" + val + "\n";
	}

	//Count
	snprintf(val, sizeof(val), "%zx", table.size());
	retval += string("    #define ") + name + "_count 0x" + val + "\n";

	return retval;
}

string ConstantTableNode::GenerateConstantsCEnum(string name, map<string, uint64_t>& table, int /*width*/)
{
	//Top
	string retval = string("enum ") + name + "\n";
	retval += "{\n";

	//Body
	char val[32];
	for(auto it : table)
	{
		snprintf(val, sizeof(val), "%lx", it.second);
		retval += string("    ") + it.first + " = 0x" + val + ",\n";
	}

	//Count
	snprintf(val, sizeof(val), "%zx", table.size());
	retval += string("    ") + name + "_count = 0x" + val + "\n";

	//Bottom
	retval += "};\n";

	return retval;
}

string ConstantTableNode::GenerateConstantsVerilogDefine(string name, map<string, uint64_t>& table, int width)
{
	//Body
	string retval;
	char val[32];
	for(auto it : table)
	{
		snprintf(val, sizeof(val), "%d'h%lx", width, it.second);
		retval += string("    `define ") + it.first + " " + val + "\n";
	}

	//Count
	snprintf(val, sizeof(val), "%d'h%zx", width, table.size());
	retval += string("    `define ") + name + "_count " + val + "\n";

	return retval;
}

string ConstantTableNode::GenerateConstantsVerilogLocalparam(string name, map<string, uint64_t>& table, int width)
{
	//Body
	string retval;
	char val[32];
	for(auto it : table)
	{
		snprintf(val, sizeof(val), "%d'h%lx", width, it.second);
		retval += string("    localparam ") + it.first + " = " + val + ";\n";
	}

	//Count
	snprintf(val, sizeof(val), "%d'h%zx", width, table.size());
	retval += string("    localparam ") + name + "_count = " + val + ";\n";

	return retval;
}

string ConstantTableNode::GetOutputBasename(string basename, string generator)
{
	//string dir = GetDirOfFile(scriptpath);

	if(generator == "c/enum")
		return basename + "_enum.h";
	else if(generator == "c/define")
		return basename + "_define.h";
	else if(generator == "verilog/define")
		return basename + "_define.vh";
	else if(generator == "verilog/localparam")
		return basename + "_localparam.vh";
	else
	{
		LogError("Invalid generator %s\n", generator.c_str());
		return "";
	}
}
