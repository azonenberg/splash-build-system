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

#ifndef BuildJob_h
#define BuildJob_h

/**
	@brief A compile, link, or other operation to be executed by a single node.
 */
class BuildJob
{
public:

	/**
		@brief Priorities for build jobs
	 */
	enum Priority
	{
		PRIO_IMMEDIATE,		//This job must run ASAP
							//Only use for very short jobs; cuts ahead of absolutely everything

		PRIO_INTERACTIVE,	//Interactive debug job. Run as soon as we have an available node.

		PRIO_NORMAL,		//Normal priority - for most builds

		PRIO_DELAYED,		//Not very important - nightly background jobs or something

		PRIO_COUNT			//Total number of priorities, used for queue management etc
	};

	BuildJob(/*Priority prio*/);
	virtual ~BuildJob();

protected:

	/**
		@brief The object which is to be generated by this job
	 */
	BuildGraphNode* m_output;

	/**
		@brief Set of input files for this job

		All of these must be in STATUS_READY before we can execute.
	 */
	std::unordered_set<BuildGraphNode*> m_inputs;

	/**
		@brief Hash of the toolchain that needs to execute this job
	 */
	std::string m_toolchainHash;

	/**
		@brief Flags this build requires
	 */
	std::unordered_set<BuildFlag> m_flags;

	/**
		@brief Priority of this build job
	 */
	Priority m_priority;
};

extern BuildJob* g_BuildJob;

#endif
