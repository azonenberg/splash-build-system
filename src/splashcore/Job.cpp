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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

Job::Job(Job::Priority prio, string toolchain)
	: m_priority(prio)
	, m_toolchainHash(toolchain)
	, m_status(STATUS_PENDING)
	, m_refcount(1)				//one ref, to our creator
	, m_ok(false)
{

}

//dtor can only be called by Unref()
Job::~Job()
{
	for(auto d : m_dependencies)
		d->Unref();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reference counting

void Job::Ref()
{
	lock_guard<mutex> lock(m_mutex);
	m_refcount ++;
}

void Job::Unref()
{
	bool last_round = false;

	{
		lock_guard<mutex> lock(m_mutex);
		m_refcount --;
		last_round = (m_refcount == 0);
	}

	if(last_round)
		delete this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

Job::Status Job::GetStatus()
{
	lock_guard<mutex> lock(m_mutex);
	return m_status;
}

void Job::SetPending()
{
	lock_guard<mutex> lock(m_mutex);
	m_status = STATUS_PENDING;
}

void Job::SetDone(bool ok)
{
	lock_guard<mutex> lock(m_mutex);
	m_status = STATUS_DONE;
	m_ok = ok;
}

void Job::SetCanceled()
{
	lock_guard<mutex> lock(m_mutex);
	m_status = STATUS_CANCELED;
}

void Job::SetRunning()
{
	lock_guard<mutex> lock(m_mutex);
	m_status = STATUS_RUNNING;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scheduling

void Job::AddDependency(Job* job)
{
	lock_guard<mutex> lock(m_mutex);

	job->Ref();
	m_dependencies.emplace(job);
}

/**
	@brief Checks if this job is runnable (all dependency jobs are complete)
 */
bool Job::IsRunnable()
{
	lock_guard<mutex> lock(m_mutex);

	//Check each of our dependencies
	for(auto d : m_dependencies)
	{
		auto status = d->GetStatus();

		//If the dependency was canceled we will never be runnable
		if(status == STATUS_CANCELED)
			return false;

		//If the dependency isn't done, no go
		if(status != STATUS_DONE)
		{
			/*
			auto bj = dynamic_cast<BuildJob*>(d);
			if(bj)
			{
				LogDebug("Job is not runnable because dependency %s is not done\n",
					bj->GetOutputNode()->GetFilePath().c_str());
			}
			*/
			return false;
		}
	}

	return true;
}

/**
	@brief Checks if this job is failed (at least one dependency job was canceled or failed)
 */
bool Job::IsCanceledByDeps()
{
	lock_guard<mutex> lock(m_mutex);

	//Check each of our dependencies
	for(auto d : m_dependencies)
	{
		auto status = d->GetStatus();

		//If the dependency was canceled we will never be runnable
		if(status == STATUS_CANCELED)
			return true;

		//If the dependency completed, but failed, we will never be runnable
		if( (status == STATUS_DONE) && (!d->IsSuccessful()) )
			return true;
	}

	return false;
}
