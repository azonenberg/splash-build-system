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

#ifndef BuildFlag_h
#define BuildFlag_h

/**
	@brief A single compiler/linker flag.
 */
class BuildFlag
{
public:
	BuildFlag(std::string flag);
	virtual ~BuildFlag();

	bool operator==(const BuildFlag& rhs) const
	{ return m_rawflag == rhs.m_rawflag; }

	/**
		@brief Bitmask of when this flag can be used
	 */
	enum FlagUsage
	{
		COMPILE_TIME 	= 0x0001,		//software compilation
		LINK_TIME 		= 0x0002,		//software linking
		SYNTHESIS_TIME	= 0x0004,		//HDL synthesis
		MAP_TIME		= 0x0008,		//HDL technology mapping
		PAR_TIME		= 0x0010,		//HDL place-and-route
		IMAGE_TIME		= 0x0020,		//Firmware image creation time
		PROOF_TIME		= 0x0040,		//Formal verification solvers etc
		ANALYSIS_TIME	= 0x0080,		//Static timing analysis etc

		FPGA_TIME		=				//All FPGA stuff
			SYNTHESIS_TIME | MAP_TIME | PAR_TIME | IMAGE_TIME | ANALYSIS_TIME,

		SCAN_TIME		= 0x0100,		//Dependency scanning (not used during actual build)

		COMPILE_AND_SCAN_TIME =			//Dependency scanning and compilation
			COMPILE_TIME | SCAN_TIME,

		NO_TIME			= 0x0000,		//placeholder
		ALL_TIME		= 0xffff
	};

	/**
		@brief Major functional group describing roughly what this flag does
	 */
	enum FlagType
	{
		TYPE_META		= 1,		//flags that control splash itself (like "global")
		TYPE_WARNING	= 2,		//enable/disable some kind of warning
		TYPE_ERROR		= 3,		//enable/disable some kind of error
		TYPE_OPTIMIZE	= 4,		//control optimization behavior
		TYPE_DEBUG		= 5,		//enable/disable debug symbols or debugging features
		TYPE_ANALYSIS	= 6,		//enable/disable profiling, tracing, etc
		TYPE_DIALECT	= 7,		//control which dialect of a language is being used
		TYPE_OUTPUT		= 8,		//control the output file (extension, soname, etc).
									//Also config bitstream parameters etc
		TYPE_LIBRARY	= 9,		//control linking of libraries, IP cores, etc
		TYPE_DEFINE		= 10,		//define macros
		TYPE_HARDWARE	= 11,		//Flags that specify the hardware being targeted.
									//This is only for "describe the silicon" type flags!
									//Do not use this category for things like boot config

		TYPE_INVALID	= 0			//placeholder
	};

	/**
		@brief Return the original flag text, human readable (like "warning/max")
	 */
	operator std::string() const
	{ return m_rawflag; }

	FlagType GetType()
	{ return m_type; }

	std::string GetFlag() const
	{ return m_flag; }

	std::string GetArgs() const
	{ return m_arg; }

	bool IsUsedAt(FlagUsage t);

protected:

	//Helpers for initializing
	void LoadWarningFlag();
	void LoadErrorFlag();
	void LoadOptimizeFlag();
	void LoadDebugFlag();
	void LoadAnalysisFlag();
	void LoadDialectFlag();
	void LoadOutputFlag();
	void LoadLibraryFlag();
	void LoadDefineFlag();
	void LoadHardwareFlag();

	/**
		@brief Usage flags (bitmask of FlagUsage)

		This is checked by toolchains to determine whether this flag is relevant to them or not
	 */
	uint32_t	m_usage;

	/**
		@brief Functional group for the flag (used to determine what part of a complex tool should look at it)
	 */
	FlagType	m_type;

	/**
		@brief Textual name of this flag (like "max" for TYPE_WARNING to enable all warnings)
	 */
	std::string	m_flag;

	/**
		@brief Raw text of this flag (used for hash comparisons etc)
	 */
	std::string	m_rawflag;

	/**
		@brief Argument of this flag (for things like libraries)
	 */
	std::string m_arg;
};

/**
	@brief Hash/comparison function (for unordered_set / set)
 */
namespace std
{
	template<> struct hash<BuildFlag>
	{
		size_t operator()(const BuildFlag& b) const
		{ return hash<string>()(static_cast<string>(b)); }
	};

	template<> struct less<BuildFlag>
	{
		size_t operator()(const BuildFlag& a, const BuildFlag& b) const
		{ return static_cast<string>(a) < static_cast<string>(b); }
	};
};

#endif

