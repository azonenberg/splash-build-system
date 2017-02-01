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

Scheduler* g_scheduler = NULL;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

Scheduler::Scheduler()
{
	m_tStart = GetTime();
	m_running = false;
}

Scheduler::~Scheduler()
{
	//LogDebug("destroying scheduler\n");

	for(auto it : m_pendingScanJobs)
	{
		for(auto job : it.second)
			job->Unref();
		it.second.clear();
	}

	for(auto it : m_runnableJobs)
	{
		for(auto job : it.second)
			job->Unref();
		it.second.clear();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// General scheduling

/**
	@brief TODO: Cancel pending jobs for that node
 */
void Scheduler::RemoveNode(clientID /*id*/)
{
	LogWarning("Scheduler::RemoveNode() not implemented\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Dependency scanning

/**
	@brief Get the next available scan job for a given node, if there is one available.

	@return A scan job, or NULL if none are available.
 */
DependencyScanJob* Scheduler::PopScanJob(clientID id)
{
	lock_guard<recursive_mutex> lock(m_mutex);
	if(m_pendingScanJobs[id].empty())
		return NULL;

	//TODO: Check if the job is runnable and wait if it's not

	auto ret = *m_pendingScanJobs[id].begin();
	m_pendingScanJobs[id].pop_front();

	ret->Ref();
	return ret;
}

/**
	@brief Get the next available job for a given node, if there is one available.

	@return A job, or NULL if none are available.
 */
Job* Scheduler::PopJob(clientID id)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	//Attempt to pop the job queue from each priority, from max to min priority
	for(int i=0; i<Job::PRIO_COUNT; i++)
	{
		Job* job = PopJob(id, static_cast<Job::Priority>(i));
		if(job)
			return job;
	}

	return NULL;
}

/**
	@brief Get the next available job for a given node and priority, if there is one available.

	@return A job, or NULL if none are available.
 */
Job* Scheduler::PopJob(clientID id, Job::Priority prio)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	//Look up the toolchains for this node
	set<string> toolchains;
	g_nodeManager->ListToolchainsForClient(toolchains, id);

	//Go over the list from oldest to newest
	jobqueue& jobs = m_runnableJobs[prio];
	int n = 0;
	for(auto it = jobs.begin(); it != jobs.end(); n++)
	{
		auto job = *it;

		//See if we have the toolchain this job needs
		if(toolchains.find(job->GetToolchain()) == toolchains.end())
		{
			//LogDebug("[%d] PopJob rejecting job %p b/c toolchain %s not offered by client %s\n",
			//	n, job, job->GetToolchain().c_str(), id.c_str());
			it++;
			continue;
		}

		//If the job had a dependency fail, delete it from the queue rather than wasting time spinning.
		if(job->IsCanceledByDeps())
		{
			//LogDebug("[%d] PopJob cancelling job %p (canceled by dependencies)\n", n, job);

			//Mark the job as canceled
			job->SetCanceled();

			//Delete it from the queue
			auto old_it = it;
			it++;
			jobs.erase(old_it);
			job->Unref();
			continue;
		}

		//If we can't run the job b/c of dependencies, keep looking
		if(!job->IsRunnable())
		{
			//LogDebug("[%d] PopJob rejecting job %p (%s) b/c not runnable (%d jobs in queue)\n",
			//	n, job, dynamic_cast<BuildJob*>(job)->GetOutputNode()->GetFilePath().c_str(), jobs.size());

			it++;
			continue;
		}

		//We're good, use this job
		jobs.erase(it);
		//LogDebug("Popped job %p (%d jobs in queue)\n", job, jobs.size());
		return job;
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Dependency scanning

/**
	@brief Schedules a dependency-scan job and returns immediately after submitting it.

	@return		The newly scheduled job
 */
DependencyScanJob* Scheduler::ScanDependenciesNonblocking(
	string fname,
	string arch,
	string toolchain,
	set<BuildFlag> flags,
	WorkingCopy* wc)
{
	//If this is our first job, reset the timer for clean profiling
	if(!m_running)
	{
		m_tStart = GetTime();
		m_running = true;
	}

	//LogDebug("[%7.3f] Scheduler::ScanDependencies (for source file %s, arch %s, toolchain %s)\n",
	//	GetDT(), fname.c_str(), arch.c_str(), toolchain.c_str() );
	//LogIndenter li;

	//Need mutex locked during scheduling so that if the golden node leaves halfway through
	//we remain in a consistent state
	lock_guard<NodeManager> lock(*g_nodeManager);

	//Look up the hash for the job
	auto hash = g_nodeManager->GetToolchainHash(arch, toolchain);
	if(hash == "")
		return NULL;
	auto chain = g_nodeManager->GetAnyToolchainForHash(hash);
	if(chain == NULL)
		return NULL;
	//LogDebug("Compiler hash is %s (%s)\n", hash.c_str(), chain->GetVersionString().c_str());

	//Find which node is supposed to run this job
	auto id = g_nodeManager->GetGoldenNodeForToolchain(hash);
	if(id.empty())
		return NULL;
	auto build_wc = g_nodeManager->GetWorkingCopy(id);
	string hostname = build_wc->GetHostname();
	//LogDebug("Golden node for this toolchain is %s (%s)\n", id.c_str(), hostname.c_str());

	//Create the scan job and submit it
	DependencyScanJob* job = new DependencyScanJob(fname, wc, hash, arch, flags);
	SubmitScanJob(id, job);
	return job;
}

/**
	@brief Waits for a scan job to complete, then crunches the results
 */
bool Scheduler::BlockOnScanResults(
	DependencyScanJob* job,
	WorkingCopy* wc,
	set<string>& deps,
	set<BuildFlag>& foundflags,
	string& errors)
{
	//Block until the job is done
	//TODO: make this more efficient
	Job::Status status = Job::STATUS_PENDING;
	while(status != Job::STATUS_DONE)
	{
		//If the job was canceled, our scan isn't going to happen so die
		status = job->GetStatus();
		if(status == Job::STATUS_CANCELED)
		{
			errors = job->GetErrors();

			LogError("Dependency scan canceled: %s\n",
				job->GetErrors().c_str());
			job->Unref();
			return false;
		}

		//Block for 2 ms (most jobs take way longer than this, the impact of waiting is miniscule)
		//TODO: have some kind of true block vs busy-polling
		usleep(2 * 1000);
	}
	//LogDebug("[%7.3f] Scan done\n", GetDT());

	//If we're done, but with errors, fail
	if(!job->IsSuccessful())
	{
		errors = job->GetErrors();
		job->Unref();
		return false;
	}

	//Add dependencies to the working copy as needed
	//Don't worry about changing anything with newly added files.
	//Note that we only care about system headers/libs, project code is already in the working copy
	auto output = job->GetOutput();
	set<string> ignored;
	for(auto it : output)
	{
		auto fname = it.first;
		if(fname.find("__sys") != string::npos)
			wc->UpdateFile(fname, it.second, false, false, ignored);
	}

	//Return the list of dependencies
	for(auto it : output)
		deps.emplace(it.first);

	//Save the set of flags
	auto ffg = job->GetFoundFlags();
	for(auto flag : ffg)
		foundflags.emplace(flag);

	//Clean up so we don't leak memory
	job->Unref();

	//LogDebug("[%7.3f] Post-processing done\n", GetDT());

	//Done
	return true;
}

/**
	@brief Submit a dependency scan job for a particular node
 */
void Scheduler::SubmitScanJob(clientID id, DependencyScanJob* job)
{
	lock_guard<recursive_mutex> lock(m_mutex);
	m_pendingScanJobs[id].push_back(job);
}

/**
	@brief Submit a job for a general build step
 */
void Scheduler::SubmitJob(Job* job)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	if(job == NULL)
		LogFatal("Submitted null job\n");
	if(job->GetToolchain() == "")
		LogFatal("Submitted job with bad toolchain\n");
	if(job->GetPriority() >= Job::PRIO_COUNT)
		LogFatal("Submitted job with bad priority\n");

	job->Ref();
	m_runnableJobs[job->GetPriority()].push_back(job);

	/*
	LogDebug("[%6.3f] Submit job %p (%s), prio %d (%d total)\n",
		GetDT(),
		job,
		dynamic_cast<BuildJob*>(job)->GetOutputNode()->GetFilePath().c_str(),
		job->GetPriority(),
		m_runnableJobs[job->GetPriority()].size());
	*/
}
