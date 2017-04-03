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

#ifndef FPGABitstreamNode_h
#define FPGABitstreamNode_h

class HDLNetlistNode;
class PhysicalNetlistNode;

/**
	@brief An FPGA bitstream
 */
class FPGABitstreamNode : public BuildGraphNode
{
public:
	FPGABitstreamNode(
		BuildGraph* graph,
		std::string arch,
		std::string config,
		std::string name,
		std::string scriptpath,
		std::string path,
		std::string toolchain,
		std::string board,
		BoardInfoFile* binfo,
		YAML::Node& node);
	virtual ~FPGABitstreamNode();

protected:
	virtual void DoFinalize();
	virtual void DoStartFinalization();

	bool GenerateConstraintFile(
		std::map<std::string, int>& pins,
		std::string path,
		BoardInfoFile* binfo);

	bool GenerateUCFConstraintFile(
		std::map<std::string, int>& pins,
		std::string path,
		BoardInfoFile* binfo,
		std::string& constraints);

	bool GeneratePCFConstraintFile(
		std::map<std::string, int>& pins,
		std::string path,
		BoardInfoFile* binfo,
		std::string& constraints);

	void UpdateHash();

	//Name of our board
	std::string m_board;

	/*
	//Internal dependency scanning stuff (used by ctor and DoFinalize only)
	std::set<std::string> m_libdeps;
	std::set<BuildFlag> m_libflags;
	*/
	std::string m_constrpath;

	std::set<BuildGraphNode*> m_sourcenodes;

	HDLNetlistNode* m_netlist;
	PhysicalNetlistNode* m_circuit;

	//Our cross-clock constraints
	typedef std::pair<std::string, std::string> stringpair;
	std::map<stringpair, std::string> m_crossclocks;
};

#endif

