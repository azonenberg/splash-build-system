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

bool OnArchListRequest(Socket& s, string& hostname, clientID id);
bool OnConfigListRequest(Socket& s, string& hostname, clientID id);
bool OnTargetListRequest(Socket& s, string& hostname, clientID id);

/**
	@brief Processes a "splash" connection
 */
void UIClientThread(Socket& s, string& hostname, clientID id)
{
	LogNotice("Developer client %s (%s) connected\n", hostname.c_str(), id.c_str());

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
	LogVerbose("    (architecture is %s)\n", dinfom.arch().c_str());

	while(true)
	{
		//Expect fileChanged or fileRemoved messages
		SplashMsg msg;
		if(!RecvMessage(s, msg, hostname))
			break;

		switch(msg.Payload_case())
		{
			case SplashMsg::kInfoRequest:
				if(!OnInfoRequest(s, msg.inforequest(), hostname, id))
					return;
				break;
			/*
			case SplashMsg::kFileRemoved:
				if(!OnFileRemoved(msg.fileremoved(), hostname, id))
					return;
				break;
			*/
			default:
				LogWarning("Connection to %s [%s] dropped (bad message type in event header)\n",
					hostname.c_str(), id.c_str());
				return;
		}
	}
}

/**
	@brief Processes a message asking for basic info about the database
 */
bool OnInfoRequest(Socket& s, const InfoRequest& msg, string& hostname, clientID id)
{
	switch(msg.type())
	{
		//arch-list request
		case InfoRequest::ARCH_LIST:
			return OnArchListRequest(s, hostname, id);

		//config-list request
		case InfoRequest::CONFIG_LIST:
			return OnConfigListRequest(s, hostname, id);

		//target-list request
		case InfoRequest::TARGET_LIST:
			return OnTargetListRequest(s, hostname, id);

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
bool OnArchListRequest(Socket& s, string& hostname, clientID id)
{
	//No need to lock the node manager etc b/c once we connect the state is refcounted and can't be deletd
	auto wc = g_nodeManager->GetWorkingCopy(id);
	BuildGraph& graph = wc->GetGraph();
	set<string> arches;
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
