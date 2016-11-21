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

#ifndef ConstantTableNode_h
#define ConstantTableNode_h

/**
	@brief A table of constants
 */
class ConstantTableNode : public BuildGraphNode
{
public:
	ConstantTableNode(
		BuildGraph* graph,
		std::string fname,
		std::string name,
		const YAML::Node& node,
		std::string table_fname,
		std::string generator,
		std::string yaml_hash);
	virtual ~ConstantTableNode();

	static std::string GetOutputBasename(std::string basename, std::string generator);

protected:
	virtual void DoFinalize();

	void GenerateConstants(
		std::string fname,
		std::string name,
		std::map<std::string, uint64_t>& table,
		std::string generator,
		int width);
	std::string GenerateConstantsCDefine(std::string name, std::map<std::string, uint64_t>& table, int width);
	std::string GenerateConstantsCEnum(std::string name, std::map<std::string, uint64_t>& table, int width);
	std::string GenerateConstantsVerilogDefine(std::string name, std::map<std::string, uint64_t>& table, int width);
	std::string GenerateConstantsVerilogLocalparam(std::string name, std::map<std::string, uint64_t>& table, int width);
};

#endif

