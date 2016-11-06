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

bool ProcessScanJob(Socket& s, string& hostname, DependencyScanJob* job, bool& ok);
bool ProcessBuildJob(Socket& s, string& hostname, Job* job, bool& ok);
bool ProcessDependencyResults(Socket& s, string& hostname, SplashMsg& msg, DependencyScanJob* job, bool& ok);
bool ProcessBulkHashRequest(Socket& s, string& hostname, SplashMsg& msg, DependencyScanJob* job);
bool ProcessBuildResults(Socket& s, string& hostname, SplashMsg& msg, Job* job, bool& ok);

void BuildClientThread(Socket& s, string& hostname, clientID id)
{
	LogNotice("Build server %s (%s) connected\n", hostname.c_str(), id.c_str());
	LogIndenter li;

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
			madd.objsuffix(),
			madd.shlibprefix()
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
			//If the job was canceled by dependencies, we cannot run it (ever)
			if(djob->IsCanceledByDeps())
			{
				djob->SetCanceled();
				djob->Unref();
				continue;
			}

			//If the job is not runnable, figure out what to do
			if(!djob->IsRunnable())
			{
				LogWarning("Job is not runnable yet!\n");
			}

			//Tell the dev client that the scan is in progress
			djob->SetRunning();

			//Push the job out to the client and run it
			bool ok = true;
			if(ProcessScanJob(s, hostname, djob, ok))
			{
				djob->SetDone(ok);
				djob->Unref();
				continue;
			}

			//Job FAILED to run (client disconnected?) - update status
			//TODO: Put it back in the queue or something fault tolerant?
			djob->SetCanceled();
			djob->Unref();
			continue;
		}

		//Look for actual compile jobs
		Job* bj = g_scheduler->PopJob(id);
		if(bj != NULL)
		{
			//If the job was canceled by dependencies, we cannot run it (ever)
			if(bj->IsCanceledByDeps())
			{
				bj->SetCanceled();
				bj->Unref();
				continue;
			}

			//If the job is not runnable, figure out what to do
			if(!bj->IsRunnable())
			{
				LogWarning("Job is not runnable yet!\n");
			}

			//We've kicked off the job, let others know
			bj->SetRunning();

			//Push the job out to the client and run it
			bool ok = true;
			if(ProcessBuildJob(s, hostname, bj, ok))
			{
				bj->SetDone(ok);
				bj->Unref();
				continue;
			}

			//Job FAILED to run (client disconnected?) - update status
			//TODO: Put it back in the queue or something fault tolerant?
			bj->SetCanceled();
			bj->Unref();
			break;
		}

		//Wait a while for more work
		usleep(100);
	}
}

/**
	@brief Runs a dependency-scan job

	@return True if we can continue. False only on unrecoverable error.
 */
bool ProcessScanJob(Socket& s, string& hostname, DependencyScanJob* job, bool& ok)
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

			//Asking for more data
			case SplashMsg::kBulkHashRequest:
				if(!ProcessBulkHashRequest(s, hostname, rxm, job))
					return false;
				break;

			//Done, we have the dependencies
			case SplashMsg::kDependencyResults:
				return ProcessDependencyResults(s, hostname, rxm, job, ok);

			//Whatever it is, it makes no sense
			default:
				LogError("Unknown / garbage message type\n");
				return false;
		}
	}
}

/**
	@brief Crunch data coming out of a DependencyResults packet

	TODO: Bulk download of files from the client as needed?

	@return True if we can continue. False only on unrecoverable error.
 */
bool ProcessDependencyResults(Socket& s, string& hostname, SplashMsg& msg, DependencyScanJob* job, bool& ok)
{
	//LogDebug("Got dependency results\n");

	//If the scan failed, we can't do anything else
	auto res = msg.dependencyresults();
	if(!res.result())
	{
		job->SetErrors(res.stdout());
		ok = false;
		return true;
	}

	//Crunch the results
	for(int i=0; i<res.deps_size(); i++)
	{
		auto dep = res.deps(i);
		string h = dep.hash();
		string f = dep.fname();
		//LogDebug("%-50s has hash %s\n", f.c_str(), h.c_str());

		if(!ValidatePath(f))
		{
			LogWarning("path %s failed to validate\n", f.c_str());
			continue;
		}

		//For each file, see if we have it in the cache already.
		//If not, ask the client for it.
		//TODO: do batched requests to cut latency
		if(!g_cache->IsCached(h))
		{
			string edat;
			if(!GetRemoteFileByHash(s, hostname, h, edat))
				return false;
			g_cache->AddFile(f, h, sha256(edat), edat, "");
		}

		//Now that the file is in cache server side, add it to the dependency list
		job->AddDependency(f, h);
	}

	//all good
	ok = true;
	return true;
}

/**
	@brief Responds to a client's request for more data
 */
bool ProcessBulkHashRequest(Socket& s, string& hostname, SplashMsg& msg, DependencyScanJob* job)
{
	//Get the request info
	auto res = msg.bulkhashrequest();

	//Pull out the working copy etc from the job
	auto wc = job->GetWorkingCopy();

	//Set up the reply
	SplashMsg resp;
	auto respm = resp.mutable_bulkhashresponse();

	//Go over the list of files and see if we can find them
	for(int i=0; i<res.fnames_size(); i++)
	{
		auto f = res.fnames(i);

		if(!ValidatePath(f))
		{
			LogWarning("path %s failed to validate\n", f.c_str());
			continue;
		}

		//Make a reply
		auto rp = respm->add_files();
		rp->set_fname(f);

		//Look it up
		string h = wc->GetFileHash(f);
		rp->set_hash(h);
		rp->set_found(h != "");
	}

	//Send it
	if(!SendMessage(s, resp, hostname))
		return false;
	return true;
}

/**
	@brief Runs a build job
 */
bool ProcessBuildJob(Socket& s, string& hostname, Job* job, bool& ok)
{
	//Make sure it's a build job (if not, it was somehow put in the wrong queue)
	BuildJob* bj = dynamic_cast<BuildJob*>(job);
	if(!bj)
	{
		LogError("ProcessBuildJob called with a non-build job\n");
		return false;
	}

	BuildGraphNode* node = bj->GetOutputNode();
	auto wc = node->GetGraph()->GetWorkingCopy();
	auto path = node->GetFilePath();

	LogDebug("Run build job %s\n", path.c_str());
	LogIndenter li;

	//Look up the flags
	set<BuildFlag> flags;
	node->GetFlagsForUseAt(bj->GetFlagUsage(), flags);

	//Build the request
	SplashMsg req;
	auto reqm = req.mutable_nodebuildrequest();
	reqm->set_arch(node->GetArch());
	reqm->set_toolchain(node->GetToolchainHash());
	for(auto src : node->GetSources())
	{
		if(!ValidatePath(src))
		{
			LogWarning("path %s failed to validate\n", src.c_str());
			continue;
		}

		string hash = wc->GetFileHash(src);
		auto dep = reqm->add_sources();
		dep->set_fname(src);
		dep->set_hash(hash);
	}
	for(auto src : node->GetDependencies())
	{
		if(!ValidatePath(src))
		{
			LogWarning("path %s failed to validate\n", src.c_str());
			continue;
		}

		string hash = wc->GetFileHash(src);
		auto dep = reqm->add_deps();
		dep->set_fname(src);
		dep->set_hash(hash);
	}
	for(auto f : flags)
		reqm->add_flags(f);
	reqm->set_fname(path);

	//Send the initial scan request to the client
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

			//Done, we have the compiled files
			case SplashMsg::kNodeBuildResults:
				return ProcessBuildResults(s, hostname, rxm, job, ok);

			//Whatever it is, it makes no sense
			default:
				LogError("Unknown / garbage message type\n");
				return false;
		}
	}

	return true;
}

/**
	@brief Deal with an incoming BuildResults message
 */
bool ProcessBuildResults(Socket& /*s*/, string& /*hostname*/, SplashMsg& msg, Job* job, bool& ok)
{
	LogDebug("Build results\n");

	auto res = msg.nodebuildresults();
	LogIndenter li;

	//Look up the build job
	BuildJob* bj = dynamic_cast<BuildJob*>(job);
	if(bj == NULL)
	{
		LogError("invalid job type (not a BuildJob)\n");
		return false;
	}
	auto node = bj->GetOutputNode();
	auto nhash = node->GetHash();

	//Pull out node properties
	string stdout = res.stdout();
	string fname = res.fname();
	string dir = GetDirOfFile(fname);
	string base = GetBasenameOfFile(fname);

	//If the build failed, add a dummy cached file with the proper ID and stdout
	//so we can query the result in the cache later on.
	ok = res.success();
	if(!ok)
	{
		g_cache->AddFailedFile(fname, nhash, stdout);
		//LogError("Build failed!\n%s\n", res.stdout().c_str());
		return true;
	}

	//Successful build if we get here

	//Go over the files and see what we have
	for(int i=0; i<res.outputs_size(); i++)
	{
		auto file = res.outputs(i);

		string ffname = file.fname();
		string hash = file.hash();
		string data = file.data();

		if(!ValidatePath(ffname))
		{
			LogWarning("path %s failed to validate\n", ffname.c_str());
			continue;
		}

		LogDebug("Compiled file %s has hash %s\n", ffname.c_str(), hash.c_str());

		//Main node output? Add to the cache using the node's hash
		string shash;
		if(GetBasenameOfFile(ffname) == base)
		{
			//LogDebug("This is the compiled output for node %s\n(path %s)\n", nhash.c_str(), fname.c_str());
			shash = nhash;
		}

		//Otherwise, add it to the cache using the content hash
		else
			shash = hash;

		//Add to the cache once we know which hash to use
		g_cache->AddFile(fname, shash, hash, data, stdout);

		//Add the node to the working copy
		node->GetGraph()->GetWorkingCopy()->UpdateFile(fname, shash, false, false);
	}

	return true;
}
