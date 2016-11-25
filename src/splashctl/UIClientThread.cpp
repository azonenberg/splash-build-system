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

bool OnInfoRequest(Socket& s, const InfoRequest& msg, string& hostname, clientID id);

bool OnBuildRequest(Socket& s, const BuildRequest& msg, string& hostname, clientID id);

bool OnArchListRequest(Socket& s, string query, string& hostname, clientID id);
bool OnClientListRequest(Socket& s, string& hostname, clientID id);
bool OnConfigListRequest(Socket& s, string& hostname, clientID id);
bool OnNodeListRequest(Socket& s, string& hostname, clientID id);
bool OnTargetListRequest(Socket& s, string& hostname, clientID id);
bool OnToolchainListRequest(Socket& s, string& hostname, clientID id);

/**
	@brief Processes a "splash" connection
 */
void UIClientThread(Socket& s, string& hostname, clientID id)
{
	LogNotice("Developer client %s (%s) connected\n", hostname.c_str(), id.c_str());

	//Set thread name
	#ifdef _GNU_SOURCE
	string threadname = ("UI:") + hostname;
	threadname.resize(15);
	pthread_setname_np(pthread_self(), threadname.c_str());
	#endif

	//Expect a DevInfo message
	SplashMsg dinfo;
	if(!RecvMessage(s, dinfo, hostname))
		return;
	if(dinfo.Payload_case() != SplashMsg::kDevInfo)
	{
		LogWarning("Connection to %s dropped (expected devInfo, got %d instead)\n",
			hostname.c_str(), dinfo.Payload_case());
		return;
	}
	auto dinfom = dinfo.devinfo();
	LogVerbose("(architecture is %s)\n", dinfom.arch().c_str());

	while(true)
	{
		//Expect fileChanged or fileRemoved messages
		SplashMsg msg;
		if(!RecvMessage(s, msg, hostname))
			break;

		switch(msg.Payload_case())
		{
			//Requesting a build
			case SplashMsg::kBuildRequest:
				if(!OnBuildRequest(s, msg.buildrequest(), hostname, id))
					return;
				break;

			//Asking for a file from our cache
			case SplashMsg::kContentRequestByHash:
				if(!ProcessContentRequest(s, hostname, msg))
					return;
				break;

			//Reuesting info about the server
			case SplashMsg::kInfoRequest:
				if(!OnInfoRequest(s, msg.inforequest(), hostname, id))
					return;
				break;

			default:
				LogWarning("Connection to %s [%s] dropped (bad message type in event header)\n",
					hostname.c_str(), id.c_str());
				return;
		}
	}
}

/**
	@brief Processes a message asking to compile something
 */
bool OnBuildRequest(Socket& s, const BuildRequest& msg, string& hostname, clientID id)
{
	bool rebuild = msg.rebuild();

	set<BuildGraphNode*> nodes;
	set<Job*> jobs;

	LogDebug("UI client: Beginning build\n");

	//Prepare the response
	SplashMsg result;
	auto resultm = result.mutable_buildresults();
	bool failed = false;

	//Dispatch the build
	{
		//Look up our graph
		auto wc = g_nodeManager->GetWorkingCopy(id);
		BuildGraph& graph = wc->GetGraph();
		lock_guard<recursive_mutex> lock(graph.GetMutex());

		//Find the targets for the requested build options
		graph.GetTargets(nodes, msg.target(), msg.arch(), msg.config());

		//See which ones are out of date
		set<BuildGraphNode*> missingtargets;
		for(auto node : nodes)
		{
			auto state = node->GetOutputState();

			switch(state)
			{
				//Done? No action required
				case NodeInfo::READY:
					continue;

				//Currently building? No action required
				case NodeInfo::BUILDING:
					continue;

				//Failed to build? We're going to fail no matter what
				case NodeInfo::FAILED:
					failed = true;
					continue;

				default:
					break;
			}


			//Nope, it's missing
			//Need to build it
			missingtargets.emplace(node);
		}

		//Now we have the list of targets that need building
		//Make them build themselves
		for(auto node : missingtargets)
		{
			LogDebug("Target %s needs to be rebuilt\n", node->GetFilePath().c_str());
			auto j = node->Build();
			if(!j)
			{
				//The build fails if at least one node cannot be built
				failed = true;
				//LogError("Failed to submit build job for node \"%s\"\n", node->GetFilePath().c_str());
				continue;
			}

			jobs.emplace(j);
		}
	}

	//Wait for build to complete
	LogDebug("UI client: Waiting for build jobs to complete\n");
	while(!jobs.empty())
	{
		//See what's finished this pass
		set<Job*> done;
		for(auto j : jobs)
		{
			auto status = j->GetStatus();
			if(status == BuildJob::STATUS_CANCELED)
			{
				done.emplace(j);
				failed = true;
			}
			else if(status == BuildJob::STATUS_DONE)
				done.emplace(j);
		}

		//Clean up finished jobs as they complete
		for(auto j : done)
		{
			jobs.erase(j);
			j->Unref();
		}

		//Wait a little while (TODO: wait until one of the jobs has finished with an event or something?)
		usleep(50 * 1000);
	}

	//LogDebug("UI client: Jobs completed\n");

	//Make a list of all nodes in the graph, even those we did not build this round
	auto wc = g_nodeManager->GetWorkingCopy(id);
	BuildGraph& graph = wc->GetGraph();
	lock_guard<recursive_mutex> lock(graph.GetMutex());

	//Create a sorted list of output paths of ALL graph nodes, even those which are not targets
	set<string> paths;
	//LogDebug("Dumping graph for wc %p\n", wc);
	for(auto it : *wc)
	{
		//string fname = it.first;
		//LogDebug("%s\n", fname.c_str());
		paths.emplace(it.first);
	}

	//Filter out those not in the build directory (TODO: can we do this faster than O(n)?)
	//and send the whole list to the client for syncing.
	//LogDebug("Build dir list\n");
	resultm->set_status(!failed);
	string bapath = graph.GetBuildArtifactPath() + "/";
	for(auto f : paths)
	{
		//Some virtual files are in the source dir (generated constant tables etc)
		//These need special treatment
		bool in_build_dir = (f.find(bapath) == 0);

		//If the file is in a system directory, skip it
		if(f.find("__sys") == 0)
			continue;

		//Look up the ID of this file
		//(not hash of the file content)
		string hash = wc->GetFileHash(f);

		//If cached successfully, add the result
		if(g_cache->IsCached(hash))
		{
			//Add the file
			//TODO: supply stdout only if asked and have client cache it?
			//TODO: set executable bit more sanely
			auto res = resultm->add_results();
			res->set_fname(f);
			res->set_hash(hash);
			res->set_stdout(g_cache->ReadCachedLog(hash));
			res->set_executable(true);
			res->set_ok(true);
			res->set_sync(in_build_dir);
		}

		//If cached fail, add that result
		else if(g_cache->IsFailed(hash))
		{
			auto res = resultm->add_results();
			res->set_fname(f);
			res->set_stdout(g_cache->ReadCachedLog(hash));
			res->set_executable(true);
			res->set_ok(false);
			res->set_sync(in_build_dir);
		}
	}

	//Go send the list to the client
	if(!SendMessage(s, result, hostname))
		return false;

	return true;
}

/**
	@brief Processes a message asking for basic info about the database
 */
bool OnInfoRequest(Socket& s, const InfoRequest& msg, string& hostname, clientID id)
{
	switch(msg.type())
	{
		//list-arch request
		case InfoRequest::ARCH_LIST:
			return OnArchListRequest(s, msg.query(), hostname, id);

		//list-client request
		case InfoRequest::CLIENT_LIST:
			return OnClientListRequest(s, hostname, id);

		//list-config request
		case InfoRequest::CONFIG_LIST:
			return OnConfigListRequest(s, hostname, id);

		//dump-graph request
		case InfoRequest::NODE_LIST:
			return OnNodeListRequest(s, hostname, id);

		//list-target request
		case InfoRequest::TARGET_LIST:
			return OnTargetListRequest(s, hostname, id);

		//list-toolchains request
		case InfoRequest::TOOLCHAIN_LIST:
			return OnToolchainListRequest(s, hostname, id);

		//Something garbage
		default:
			LogWarning("Connection to %s [%s] dropped (bad InfoRequest type)\n",
					hostname.c_str(), id.c_str());
			return false;
	}

	return true;
}

/**
	@brief Processes a "splash list-arches" request
 */
bool OnArchListRequest(Socket& s, string query, string& hostname, clientID id)
{
	//No need to lock the node manager etc b/c once we connect the state is refcounted and can't be deletd
	auto wc = g_nodeManager->GetWorkingCopy(id);
	BuildGraph& graph = wc->GetGraph();
	set<string> arches;
	if(query != "")
		graph.GetArches(arches, query);
	else
		graph.GetArches(arches);

	//Go send the list to the client
	SplashMsg result;
	auto resultm = result.mutable_archlist();
	for(auto a : arches)
		resultm->add_arches(a);
	if(!SendMessage(s, result, hostname))
		return false;

	return true;
}

/**
	@brief Processes a "splash list-clients" request
 */
bool OnClientListRequest(Socket& s, string& hostname, clientID /*id*/)
{
	//Prep the result
	SplashMsg result;
	auto resultm = result.mutable_clientlist();

	//Need to lock node manager during interaction in case other stuff comes/goes
	g_nodeManager->lock();

		set<clientID> clients;
		g_nodeManager->ListClients(clients);

		//Get extended data about each clientID
		for(auto uuid : clients)
		{
			//Look up info about the working copy
			auto wc = g_nodeManager->GetWorkingCopy(uuid);
			string hostname = wc->GetHostname();

			//TODO: can we have >1 of a given type of client?
			int ndev = wc->GetClientCount(ClientHello::CLIENT_DEVELOPER);
			int nbuild = wc->GetClientCount(ClientHello::CLIENT_BUILD);
			int nui = wc->GetClientCount(ClientHello::CLIENT_UI);

			for(int i=0; i<ndev; i++)
			{
				auto c = resultm->add_infos();
				c->set_type(ClientHello::CLIENT_DEVELOPER);
				c->set_hostname(hostname);
				c->set_uuid(uuid);
			}

			for(int i=0; i<nbuild; i++)
			{
				auto c = resultm->add_infos();
				c->set_type(ClientHello::CLIENT_BUILD);
				c->set_hostname(hostname);
				c->set_uuid(uuid);
			}

			for(int i=0; i<nui; i++)
			{
				auto c = resultm->add_infos();
				c->set_type(ClientHello::CLIENT_UI);
				c->set_hostname(hostname);
				c->set_uuid(uuid);
			}

		}

	g_nodeManager->unlock();

	//Go send the list to the client
	if(!SendMessage(s, result, hostname))
		return false;

	return true;
}

/**
	@brief Processes a "splash list-configs" request
 */
bool OnConfigListRequest(Socket& s, string& hostname, clientID id)
{
	//No need to lock the node manager etc b/c once we connect the state is refcounted and can't be deletd
	auto wc = g_nodeManager->GetWorkingCopy(id);
	BuildGraph& graph = wc->GetGraph();
	set<string> configs;
	graph.GetConfigs(configs);

	//Go send the list to the client
	SplashMsg result;
	auto resultm = result.mutable_configlist();
	for(auto c : configs)
		resultm->add_configs(c);
	if(!SendMessage(s, result, hostname))
		return false;

	return true;
}

/**
	@brief Processes a "splash dump-graph" request
 */
bool OnNodeListRequest(Socket& s, string& hostname, clientID id)
{
	//Prep the result
	SplashMsg result;
	auto resultm = result.mutable_nodelist();

	//Need to lock node manager during interaction in case other stuff comes/goes
	g_nodeManager->lock();

		auto wc = g_nodeManager->GetWorkingCopy(id);
		BuildGraph& graph = wc->GetGraph();

		set<string> hashes;
		graph.GetNodes(hashes);

		//Get detailed info about each node
		for(auto h : hashes)
		{
			auto node = graph.GetNodeWithHash(h);

			auto info = resultm->add_infos();
			info->set_hash(h);
			info->set_path(node->GetFilePath());
			info->set_toolchain(node->GetToolchain());
			info->set_arch(node->GetArch());
			info->set_config(node->GetConfig());
			info->set_script(node->GetScript());
			info->set_state(g_cache->GetState(h));

			//Get the dependency list
			auto deps = node->GetDependencies();
			for(auto d : deps)
				info->add_deps(wc->GetFileHash(d));
		}

	g_nodeManager->unlock();

	//Go send the list to the client
	if(!SendMessage(s, result, hostname))
		return false;

	return true;
}

/**
	@brief Processes a "splash list-targets" request
 */
bool OnTargetListRequest(Socket& s, string& hostname, clientID id)
{
	//No need to lock the node manager etc b/c once we connect the state is refcounted and can't be deletd
	auto wc = g_nodeManager->GetWorkingCopy(id);
	BuildGraph& graph = wc->GetGraph();
	set<string> targets;
	graph.GetTargets(targets);

	//Go send the list to the client
	SplashMsg result;
	auto resultm = result.mutable_targetlist();
	for(auto t : targets)
	{
		auto info = resultm->add_info();
		info->set_name(t);
		info->set_script(graph.GetTargetScript(t));
		info->set_toolchain(graph.GetTargetToolchain(t));
	}
	if(!SendMessage(s, result, hostname))
		return false;

	return true;
}

/**
	@brief Processes a "splash list-toolchains" request
 */
bool OnToolchainListRequest(Socket& s, string& hostname, clientID /*id*/)
{
	//Prep the result
	SplashMsg result;
	auto resultm = result.mutable_toolchainlist();

	//Need to lock node manager during iteraction in case other stuff comes/goes
	g_nodeManager->lock();

		set<clientID> hashes;
		g_nodeManager->ListToolchains(hashes);

		//Get extended data about each toolchain
		for(auto hash : hashes)
		{
			auto info = resultm->add_infos();
			info->set_hash(hash);

			//Look up a toolchain instance so we can query it
			auto chain = g_nodeManager->GetAnyToolchainForHash(hash);
			info->set_version(chain->GetVersionString());

			set<string> names;
			g_nodeManager->ListNamesForToolchain(names, hash);
			for(auto n : names)
				info->add_names(n);

			set<string> clients;
			g_nodeManager->ListClientsForToolchain(clients, hash);
			for(auto c : clients)
				info->add_uuids(c);

			set<string> langs;
			chain->GetSupportedLanguages(langs);
			for(auto l : langs)
				info->add_langs(l);

			auto triplets = chain->GetTargetTriplets();
			for(auto t : triplets)
				info->add_arches(t);
		}

	g_nodeManager->unlock();

	//Go send the list to the client
	if(!SendMessage(s, result, hostname))
		return false;

	return true;
}
