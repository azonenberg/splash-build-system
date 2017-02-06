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

#ifndef XilinxISEToolchain_h
#define XilinxISEToolchain_h

/**
	@brief A Xilinx ISE FPGA toolchain
 */
class XilinxISEToolchain : public FPGAToolchain
{
public:
	XilinxISEToolchain(std::string basepath, int major, int minor);
	virtual ~XilinxISEToolchain();

	virtual bool Build(
		std::string triplet,
		std::set<std::string> sources,
		std::string fname,
		std::set<BuildFlag> flags,
		std::map<std::string, std::string>& outputs,
		std::string& stdout);

protected:

	bool GetTargetPartName(
		std::set<BuildFlag> flags,
		std::string& triplet,
		std::string& part,
		std::string& stdout,
		std::string& fname,
		int& speed);

	bool Synthesize(
		std::string triplet,
		std::set<std::string> sources,
		std::string fname,
		std::set<BuildFlag> flags,
		std::map<std::string, std::string>& outputs,
		std::string& stdout);
	std::string FlagToStringForSynthesis(BuildFlag flag);
	void CrunchSynthesisLog(const std::string& log, std::string& stdout);

	bool MapAndPar(
		std::string triplet,
		std::set<std::string> sources,
		std::string fname,
		std::set<BuildFlag> flags,
		std::map<std::string, std::string>& outputs,
		std::string& stdout);

	bool Translate(
		std::string triplet,
		std::set<std::string> sources,
		std::string fname,
		std::set<BuildFlag> flags,
		std::map<std::string, std::string>& outputs,
		std::string& stdout);
	void CrunchTranslateLog(const std::string& log, std::string& stdout);

	bool Map(
		std::string triplet,
		std::set<std::string> sources,
		std::string fname,
		std::set<BuildFlag> flags,
		std::map<std::string, std::string>& outputs,
		std::string& stdout);
	void CrunchMapLog(const std::string& log, std::string& stdout);

	bool Par(
		std::string triplet,
		std::set<std::string> sources,
		std::string fname,
		std::set<BuildFlag> flags,
		std::map<std::string, std::string>& outputs,
		std::string& stdout);
	void CrunchParLog(const std::string& log, std::string& stdout);

	bool GenerateBitstream(
		std::string triplet,
		std::set<std::string> sources,
		std::string fname,
		std::set<BuildFlag> flags,
		std::map<std::string, std::string>& outputs,
		std::string& stdout);
	void CrunchBitgenLog(const std::string& log, std::string& stdout);

	bool StaticTiming(
		std::string triplet,
		std::set<std::string> sources,
		std::string fname,
		std::set<BuildFlag> flags,
		std::map<std::string, std::string>& outputs,
		std::string& stdout);
	void CrunchTimingLog(const std::string& log, std::string& stdout);

	std::string m_binpath;
};

#endif

