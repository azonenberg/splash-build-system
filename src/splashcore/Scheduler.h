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
	blocking on the first prereq. Once this is met either block on another prereq or move to the ready queue.

	If the job is locked to a specific node (dependency scanning), put it in the queue for that node. Each node has one
	queue for each priority level.

	If the job is not locked, put it in the global run queue.

	When a node is ready for work, it checks the per-node run queue for max priority jobs, then the global run queue
	for max priority jobs, then continues down (so per-node jobs have priority over global jobs).
 */
class Scheduler
{
public:
	Scheduler();
	virtual ~Scheduler();

protected:

	/**
		@brief Waiting list (jobs we cannot yet run b/c some prereqs are not met)
	 */

	/**
		@brief Jobs currently executing on each node (need to push back to main run queue if the node disconnects)
	 */

	/**
		@brief Jobs which are locked to a specific node, sorted by priority
	 */

	/**
		@brief Jobs which are currently eligible to run, sorted by priority
	 */
};

extern Scheduler* g_scheduler;

#endif
