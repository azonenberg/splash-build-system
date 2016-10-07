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

#include "splashctl.h"

using namespace std;

bool ProcessScanJob(Socket& s, string& hostname, DependencyScanJob* job);
bool ProcessBuildJob(Socket& s, string& hostname, Job* job);
bool ProcessDependencyResults(Socket& s, string& hostname, SplashMsg& msg, DependencyScanJob* job);

void BuildClientThread(Socket& s, string& hostname, clientID id)
{
	LogNotice("Build server %s (%s) connected\n", hostname.c_str(), id.c_str());

	//Expect a BuildInfo message
	SplashMsg binfo;
	if(!RecvMessage(s, binfo, hostname))
		return;
	if(binfo.Payload_case() != SplashMsg::kBuildInfo)
	{
		LogWarning("Connection to %s dropped (expected buildInfo, got %d instead)\n",
			hostname.c_str(), binfo.Payload_case());
		return;
	}
	auto binfom = binfo.buildinfo();

	//Print stats
	LogVerbose("Build server %s has %d CPU cores, speed %d, RAM capacity %d MB, %d toolchains installed\n",
		hostname.c_str(),
		binfom.cpucount(),
		binfom.cpuspeed(),
		binfom.ramsize(),
		binfom.numchains()
		);

	//If no toolchains, just quit now
	if(binfom.numchains() == 0)
	{
		LogWarning("Connection from %s dropped (no toolchains found)\n", hostname.c_str());
		return;
	}

	//Read the toolchains
	for(unsigned int i=0; i<binfom.numchains(); i++)
	{
		//Get the message
		SplashMsg tadd;
		if(!RecvMessage(s, tadd, hostname))
			return;
		auto madd = tadd.addcompiler();

		//Create and initialize the toolchain object
		RemoteToolchain* toolchain = new RemoteToolchain(
			static_cast<RemoteToolchain::ToolchainType>(madd.compilertype()),
			madd.hash(),
			madd.versionstr(),
			(madd.versionnum() >> 16) & 0xffff,
			(madd.versionnum() >> 8) & 0xff,
			(madd.versionnum() >> 0) & 0xff,
			madd.exesuffix(),
			madd.shlibsuffix(),
			madd.stlibsuffix(),
			madd.objsuffix()
			);

		//Languages
		for(int j=0; j<madd.lang_size(); j++)
			toolchain->AddLanguage(static_cast<Toolchain::Language>(madd.lang(j)));

		//Triplets
		for(int j=0; j<madd.triplet_size(); j++)
			toolchain->AddTriplet(madd.triplet(j));

		//Register the toolchain in the global indexes
		bool moreToolchains = (i+1 < binfom.numchains());
		g_nodeManager->AddToolchain(id, toolchain, moreToolchains);
	}

	while(true)
	{
		//See if we have any scan jobs to process
		DependencyScanJob* djob = g_scheduler->PopScanJob(id);
		if(djob != NULL)
		{
			//Tell the dev client that the scan is in progress
			djob->SetRunning();

			//Push the job out to the client and run it
			if(ProcessScanJob(s, hostname, djob))
			{
				djob->SetDone();
				djob->Unref();
				continue;
			}

			//Job FAILED to run (client disconnected?) - update status
			djob->SetCanceled();
			djob->Unref();
			continue;
		}

		//Look for actual compile jobs
		Job* bjob = g_scheduler->PopJob(id);
		if(bjob != NULL)
		{
			//We've kicked off the job, let others know
			bjob->SetRunning();

			//Push the job out to the client and run it
			if(ProcessBuildJob(s, hostname, bjob))
			{
				bjob->SetDone();
				bjob->Unref();
				continue;
			}

			//Job FAILED to run (client disconnected?) - update status
			//TODO: Put it back in the queue or something fault tolerant?
			bjob->SetCanceled();
			bjob->Unref();
			continue;
		}

		//Wait a while for more work
		usleep(100);
	}
}

/**
	@brief Runs a dependency-scan job
 */
bool ProcessScanJob(Socket& s, string& hostname, DependencyScanJob* job)
{
	//Grab the job settings
	string chain = job->GetToolchain();
	string path = job->GetPath();
	auto wc = job->GetWorkingCopy();
	string hash = wc->GetFileHash(path);
	auto flags = job->GetFlags();

	//Send the initial scan request to the client
	SplashMsg req;
	auto reqm = req.mutable_dependencyscan();
	reqm->set_toolchain(chain);
	reqm->set_fname(path);
	reqm->set_hash(hash);
	reqm->set_arch(job->GetArch());
	for(auto f : flags)
		reqm->add_flags(f);
	if(!SendMessage(s, req, hostname))
		return false;

	//Let the client do its thing
	while(true)
	{
		//Get a response. It's either "done" or "get more files"
		SplashMsg rxm;
		if(!RecvMessage(s, rxm, hostname))
			return false;

		auto type = rxm.Payload_case();

		switch(type)
		{
			//Asking for more data
			case SplashMsg::kContentRequestByHash:
				if(!ProcessContentRequest(s, hostname, rxm))
					return false;
				break;

			//Done, we have the dependencies
			case SplashMsg::kDependencyResults:
				if(!ProcessDependencyResults(s, hostname, rxm, job))
					return false;
				return true;

			//Whatever it is, it makes no sense
			default:
				LogError("Unknown / garbage message type\n");
				return false;
		}
	}
}

/**
	@brief Crunch data coming out of a DependencyResults packet
 */
bool ProcessDependencyResults(Socket& s, string& hostname, SplashMsg& msg, DependencyScanJob* job)
{
	//If the scan failed, we can't do anything else
	auto res = msg.dependencyresults();
	if(!res.result())
		return false;

	//Crunch the results
	for(int i=0; i<res.deps_size(); i++)
	{
		auto dep = res.deps(i);
		string h = dep.hash();
		string f = dep.fname();
		//LogDebug("    %-50s has hash %s\n", f.c_str(), h.c_str());

		//For each file, see if we have it in the cache already.
		//If not, ask the client for it.
		//TODO: do batched requests to cut latency
		if(!g_cache->IsCached(h))
		{
			string edat;
			if(!GetRemoteFileByHash(s, hostname, h, edat))
				return false;
			g_cache->AddFile(f, h, sha256(edat), edat.c_str(), edat.size());
		}

		//Now that the file is in cache server side, add it to the dependency list
		job->AddDependency(f, h);
	}

	//all good
	return true;
}

/**
	@brief Runs a build job
 */
bool ProcessBuildJob(Socket& s, string& hostname, Job* job)
{
	//Make sure it's a build job (if not, it was somehow put in the wrong queue)
	BuildJob* bjob = dynamic_cast<BuildJob*>(job);
	if(!bjob)
	{
		LogError("ProcessBuildJob called with a non-build job\n");
		return false;
	}

	//TODO: Push it to the client

	//TODO: Get results

	//TODO: Update server-side cache

	//TODO: Unblock any jobs that are pending

	auto node = bjob->GetOutputNode();
	auto path = node->GetFilePath();
	LogDebug("TODO: Run build job %p (for %s)\n", job, path.c_str());
	return true;
}
