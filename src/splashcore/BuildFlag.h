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
		COMPILE_TIME 	= 0x01,		//software compilation
		LINK_TIME 		= 0x02,		//software linking
		SYNTHESIS_TIME	= 0x04,		//HDL synthesis
		MAP_TIME		= 0x08,		//HDL technology mapping
		PAR_TIME		= 0x10,		//HDL place-and-route
		IMAGE_TIME		= 0x20,		//Firmware image creation time
		
		NO_TIME			= 0x00		//placeholder
	};
	
	/**
		@brief Major functional group describing roughly what this flag does
	 */
	enum FlagType
	{
		TYPE_WARNING	= 1,		//enable/disable some kind of warning
		TYPE_ERROR		= 2,		//enable/disable some kind of error
		TYPE_OPTIMIZE	= 3,		//control optimization behavior
		TYPE_DEBUG		= 4,		//enable/disable debug symbols or debugging features
		TYPE_ANALYSIS	= 5,		//enable/disable profiling, tracing, etc
		TYPE_DIALECT	= 6,		//control which dialect of a language is being used
		
		TYPE_INVALID	= 0			//placeholder
	};
	
	operator std::string() const
	{ return m_rawflag; }
	
protected:
	
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
};

/**
	@brief Hash function (for unordered_set)
 */
namespace std
{
	template<> struct hash<BuildFlag>
	{
		size_t operator()(const BuildFlag& b) const
		{ return hash<string>()(static_cast<string>(b)); }
	};
};

#endif
