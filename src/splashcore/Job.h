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

#ifndef Job_h
#define Job_h

/**
	@brief A job which needs to run on a build node
 */
class Job
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

	Job(Priority prio, std::string toolchain);

protected:
	//must delete via refcounter
	virtual ~Job();

public:

	/**
		@brief Gets the priority of this job.

		No mutexing needed as this is static after object creation.
	 */
	Priority GetPriority()
	{ return m_priority; }

	enum Status
	{
		STATUS_PENDING,		//The job is in line waiting to run.
		STATUS_RUNNING,		//The job is currently running.
		STATUS_DONE,		//The job completed running (may or may not have been successful)
		STATUS_CANCELED		//The job was not run to completion. Most likely, the assigned node dropped offline.
	};

	Status GetStatus();

	void SetPending();
	void SetDone(bool ok);
	void SetCanceled();
	void SetRunning();

	//Reference counting for job status etc
	void Ref();
	void Unref();

	std::string GetToolchain()
	{ return m_toolchainHash; }

	void AddDependency(Job* job);

	virtual bool IsRunnable();
	bool IsCanceledByDeps();

	bool IsSuccessful()
	{ return m_ok; }

protected:

	/// @brief The mutex used to synchronize updates
	std::mutex m_mutex;

	/**
		@brief Priority of this build job
	 */
	Priority m_priority;

	/// @brief Toolchain hash we asked for
	std::string m_toolchainHash;

	/// @brief Job status
	Status m_status;

	/// @brief Reference count
	int m_refcount;

	/// @brief Jobs we depend on
	std::set<Job*> m_dependencies;

	/// @brief True if we completed successfully
	bool m_ok;
};

#endif

