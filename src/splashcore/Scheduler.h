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

#ifndef Scheduler_h
#define Scheduler_h

/**
	@brief Server-side scheduler for pushing BuildJob's to nodes

	Basic design:

	As a job comes in, determine if all of the pre-requisites have been met. If not, put it in the waiting list
	blocking on the first prereq. Once this is met either block on another prereq or move to the run queue.

	When a node is ready for work, it checks the the global run queue in decreasing priority order.
 */
class Scheduler
{
public:
	Scheduler();
	virtual ~Scheduler();

	//Build graph interface
	bool ScanDependencies(
		std::string fname,
		std::string arch,
		std::string toolchain,
		std::set<BuildFlag> flags,
		WorkingCopy* wc,
		std::set<std::string>& deps);

	//Node creation/deletion
	void RemoveNode(clientID id);

	//Internal helpers
	void SubmitScanJob(clientID id, DependencyScanJob* job);
	void SubmitJob(Job* job);

	//Node interface
	DependencyScanJob* PopScanJob(clientID id);
	Job* PopJob(clientID id);
	Job* PopJob(clientID id, Job::Priority prio);

protected:

	/**
		@brief Mutex used for blocking
	 */
	std::recursive_mutex m_mutex;

	//A FIFO of scan jobs waiting to run
	typedef std::list<DependencyScanJob*> scanqueue;

	//A FIFO of jobs waiting to run
	typedef std::list<Job*> jobqueue;

	/**
		@brief Dependency scan jobs waiting to run
	 */
	std::map<clientID, scanqueue> m_pendingScanJobs;

	/**
		@brief Waiting list (jobs we cannot yet run b/c some prereqs are not met)
	 */

	/**
		@brief Jobs currently executing on each node (need to push back to main run queue if the node disconnects)
	 */

	/**
		@brief Jobs which are currently eligible to run, grouped by priority.

		Within each queue jobs are sorted in FIFO order. Note that jobs may run out-of-order because
		not all jobs can run on any given node.
	 */
	std::map<Job::Priority, jobqueue> m_runnableJobs;
};

extern Scheduler* g_scheduler;

#endif
