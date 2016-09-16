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
}

Scheduler::~Scheduler()
{
	LogDebug("destroying scheduler\n");
	for(auto it : m_pendingScanJobs)
	{
		for(auto job : it.second)
			delete job;
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

	auto ret = *m_pendingScanJobs[id].begin();
	m_pendingScanJobs[id].pop_front();
	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Dependency scanning

/**
	@brief Immediately schedules a dependency-scan job and blocks until it completes.

	@return		True on a successful scan
				False if the scan could not complete (parse error, 
 */
bool Scheduler::ScanDependencies(string fname, string config, string arch, string toolchain)
{
	LogDebug("        Scheduler::ScanDependencies (for source file %s, config %s, arch %s, toolchain %s)\n",
		fname.c_str(), config.c_str(), arch.c_str(), toolchain.c_str() );

	//Need mutex locked during scheduling so that if the golden node leaves halfway through
	//we remain in a consistent state
	lock_guard<NodeManager> lock(*g_nodeManager);

	//Look up the hash for the job
	auto hash = g_nodeManager->GetToolchainHash(arch, toolchain);
	if(hash == "")
		return false;
	auto chain = g_nodeManager->GetAnyToolchainForHash(hash);
	if(chain == NULL)
		return false;
	LogDebug("            Compiler hash is %s (%s)\n", hash.c_str(), chain->GetVersionString().c_str());

	//Find which node is supposed to run this job
	auto id = g_nodeManager->GetGoldenNodeForToolchain(hash);
	if(id == 0)
		return false;
	string hostname = g_nodeManager->GetWorkingCopy(id)->GetHostname();
	LogDebug("            Golden node for this toolchain is %d (%s)\n", (int)id, hostname.c_str());

	//Create the scan job and submit it
	//TODO: pass real args
	DependencyScanJob* job = new DependencyScanJob;
	SubmitScanJob(id, job);

	//Block until the job is done
	//TODO: make this more efficient
	Job::Status status = Job::STATUS_PENDING;
	while(status != Job::STATUS_DONE)
	{
		//If the job was canceled, our scan isn't going to happen so die
		//TODO: Delete the job so we don't leak memory
		status = job->GetStatus();
		if(status == Job::STATUS_CANCELED)
			return false;

		//Block for 250 us (typical slow LAN round trip time)
		usleep(250);
	}

	LogDebug("Job done\n");

	//TODO: Delete the job so we don't leak memory

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
