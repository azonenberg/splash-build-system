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
#include <poll.h>

using namespace std;

bool ProcessScanJob(Socket& s, string& hostname, DependencyScanJob* job, bool& ok);
bool ProcessBuildJob(Socket& s, string& hostname, Job* job, bool& ok);
bool ProcessDependencyResults(Socket& s, string& hostname, SplashMsg& msg, DependencyScanJob* job, bool& ok);
bool ProcessBulkHashRequest(Socket& s, string& hostname, SplashMsg& msg, DependencyScanJob* job);
bool ProcessBuildResults(Socket& s, string& hostname, SplashMsg& msg, Job* job, bool& ok);

void BuildClientThread(Socket& s, string& hostname, clientID id)
{
	LogNotice("Build server %s (%s) connected\n", hostname.c_str(), id.c_str());

	//Set thread name
	#ifdef _GNU_SOURCE
	string threadname = ("BLD:") + hostname;
	threadname.resize(15);
	pthread_setname_np(pthread_self(), threadname.c_str());
	#endif

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

		//Look up the pre/suffixes for various file types
		Toolchain::stringpairmap fixes;
		for(int j=0; j<madd.types_size(); j++)
		{
			auto t = madd.types(j);
			fixes[t.name()] = Toolchain::stringpair(t.prefix(), t.suffix());
		}

		//Create and initialize the toolchain object
		RemoteToolchain* toolchain = new RemoteToolchain(
			static_cast<RemoteToolchain::ToolchainType>(madd.compilertype()),
			madd.hash(),
			madd.versionstr(),
			(madd.versionnum() >> 16) & 0xffff,
			(madd.versionnum() >> 8) & 0xff,
			(madd.versionnum() >> 0) & 0xff,
			fixes
			);

		//Languages
		for(int j=0; j<madd.lang_size(); j++)
			toolchain->AddLanguage(static_cast<Toolchain::Language>(madd.lang(j)));

		//Triplets
		for(int j=0; j<madd.triplet_size(); j++)
			toolchain->AddTriplet(madd.triplet(j));

		//LogDebug("Toolchain %s\n", madd.versionstr().c_str());
		//LogIndenter li;

		//Register the toolchain in the global indexes
		bool moreToolchains = (i+1 < binfom.numchains());
		g_nodeManager->AddToolchain(id, toolchain, moreToolchains);
	}

	//double lastJob = -1;
	while(true)
	{
		//See if we have any scan jobs to process
		DependencyScanJob* djob = g_scheduler->PopScanJob(id);
		if(djob != NULL)
		{
			/*
			double dt = 0;
			if(lastJob > 0)
				dt = GetTime() - lastJob;
			LogDebug("[%7.3f] BuildClientThread %s got job (idle for %.3f)\n",
				g_scheduler->GetDT(), hostname.c_str(), dt);
			*/

			//If the job was canceled by dependencies, we cannot run it (ever)
			if(djob->IsCanceledByDeps())
			{
				djob->SetCanceled();
				djob->Unref();
				continue;
			}

			//If the job is not runnable, block until it is
			while(!djob->IsRunnable())
			{
				LogWarning("Job is not runnable yet!\n");
				usleep(50 * 1000);
			}

			//Tell the dev client that the scan is in progress
			djob->SetRunning();

			//Push the job out to the client and run it
			bool ok = true;
			if(ProcessScanJob(s, hostname, djob, ok))
			{
				djob->SetDone(ok);
				djob->Unref();
				//LogDebug("[%7.3f] Job completed\n", g_scheduler->GetDT());
				//lastJob = GetTime();
				g_nodeManager->RemoveJob(id, djob);
				continue;
			}

			//Job FAILED to run! The client probably disconnected.
			//Abort, let Scheduler::RemoveNode reschedule it
			return;
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
				g_nodeManager->RemoveJob(id, bj);
				continue;
			}

			//If the job is not runnable, figure out what to do
			//For now, just block
			while(!bj->IsRunnable())
			{
				LogWarning("Job is not runnable yet!\n");

				pollfd pfd;
				pfd.fd = s;
				pfd.events = POLLRDHUP;
				if(0 != poll(&pfd, 1, 2))
					return;
			}

			//We've kicked off the job, let others know
			bj->SetRunning();

			//Push the job out to the client and run it
			bool ok = true;
			if(ProcessBuildJob(s, hostname, bj, ok))
			{
				bj->SetDone(ok);
				bj->Unref();
				g_nodeManager->RemoveJob(id, bj);
				continue;
			}

			//Job FAILED to run! The client probably disconnected.
			//Abort, let Scheduler::RemoveNode reschedule it
			return;
		}

		//Check if our client disconnected. Wait up to 50 ms for disconnection to avoid busy-polling
		pollfd pfd;
		pfd.fd = s;
		pfd.events = POLLRDHUP;
		if(0 != poll(&pfd, 1, 50))
			return;

		if(g_quitting)
			break;
	}
}

/**
	@brief Runs a dependency-scan job

	@return True if we can continue. False only on unrecoverable error.
 */
bool ProcessScanJob(Socket& s, string& hostname, DependencyScanJob* job, bool& ok)
{
	//LogDebug("[%7.3f] ProcessScanJob on %s\n", g_scheduler->GetDT(), hostname.c_str());

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

	//LogDebug("[%7.3f] BuildClientThread job pushed out\n", g_scheduler->GetDT());

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

	@param ok			Indicates success/failure of this scan

	@return True if we can continue. False only on unrecoverable error.
 */
bool ProcessDependencyResults(Socket& s, string& hostname, SplashMsg& msg, DependencyScanJob* job, bool& ok)
{
	//LogDebug("[%7.3f] BuildClientThread got dependency results (for %s)\n",
	//	g_scheduler->GetDT(), job->GetPath().c_str());
	LogIndenter li;

	//If the scan failed, we can't do anything else
	auto res = msg.dependencyresults();
	if(!res.result())
	{
		job->SetErrors(res.stdout());
		ok = false;
		return true;
	}

	//Map of fname -> hash pairs
	map<string, string> hashes;

	//Crunch the results
	for(int i=0; i<res.deps_size(); i++)
	{
		auto dep = res.deps(i);
		string h = dep.hash();
		string f = dep.fname();

		if(!ValidatePath(f))
		{
			LogWarning("path %s failed to validate\n", f.c_str());
			ok = false;
			continue;
		}

		//Canonicalize the path
		if(!CanonicalizePathThatMightNotExist(f))
		{
			LogWarning("path %s failed to canonicalize\n", f.c_str());
			ok = false;
			continue;
		}

		//If the file is in a system directory, pull it from the client.
		//DO NOT pull source files from the client, we already have them.
		if(f.find("__sys") != string::npos)
			hashes[f] = h;

		//Debug: print source file dependency names
		//else
		//	LogDebug("%-50s has hash %s\n", f.c_str(), h.c_str());

		//Add dependency even if we don't have the content for the hash yet
		//since nobody will try de-referencing the hash before we report the job as complete
		job->AddDependency(f, h);
	}

	//Add the flags we found
	for(int i=0; i<res.libflags_size(); i++)
		job->AddFoundFlag(BuildFlag(res.libflags(i)));

	//Pull the files into the cache
	if(!RefreshRemoteFilesByHash(s, hostname, hashes))
	{
		ok = false;
		return false;
	}

	//all good
	//LogDebug("[%7.3f] BuildClientThread results done\n", g_scheduler->GetDT());
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
		bool found = wc->HasFile(f);
		if(found)
			rp->set_hash(wc->GetFileHash(f));
		rp->set_found(found);
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

	LogDebug("[%6.3f] Run build job %p (%s)\n", g_scheduler->GetDT(), job, path.c_str());
	//LogIndenter li;

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

	//Sanity check: node must have at least one source and one dependency
	if( (reqm->sources_size() == 0) || (reqm->deps_size() == 0) )
	{
		LogError("Node has no sources or no dependencies! This shouldn't be possible\n");
		return false;
	}

	//Send the request to the client
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
	auto res = msg.nodebuildresults();
	ok = res.success();
	//LogIndenter li;

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

	LogDebug("[%6.3f] Build results (for %s)\n", g_scheduler->GetDT(), fname.c_str());

	//Go over the files and see what we have
	//Note that we want to process things EVEN IF THE BUILD FAILS so that the client can get logs etc
	set<string> ignored;
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

		//Get the full path of the file
		ffname = dir + "/" + ffname;
		//LogDebug("Compiled file %s has hash %s\n", ffname.c_str(), hash.c_str());

		//Main node output? Add to the cache using the node's hash (and use the stdout)
		string shash;
		string sstdout;
		if(GetBasenameOfFile(ffname) == base)
		{
			//Include the binary even if it's a fail (might be partially routed netlist etc we can debug with)

			//LogDebug("This is the compiled output for node %s\n(path %s)\n", nhash.c_str(), fname.c_str());
			shash = nhash;
			sstdout = stdout;
		}

		//Otherwise, add it to the cache using the content hash
		else
			shash = hash;

		//Add to the cache once we know which hash to use
		g_cache->AddFile(ffname, shash, hash, data, sstdout);

		//Add the node to the working copy
		//Don't dirty any new build scripts, we only care about that when we change a script
		//LogDebug("Adding %s to wc %p as %s\n", ffname.c_str(), node->GetGraph()->GetWorkingCopy(), shash.c_str());
		node->GetGraph()->GetWorkingCopy()->UpdateFile(ffname, shash, false, false, ignored);
	}

	//If the build failed, add a dummy cached file with the proper ID and stdout
	//so we can query the result in the cache later on.
	if(!ok)
	{
		//If there was no stdout, print a generic error message
		if(stdout == "")
			stdout = "ERROR: Build step failed with no stdout\n";

		g_cache->AddFailedFile(fname, nhash, stdout);
		//LogError("Build failed!\n%s\n", res.stdout().c_str());
		return true;
	}

	return true;
}
