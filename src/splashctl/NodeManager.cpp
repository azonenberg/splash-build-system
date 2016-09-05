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

NodeManager* g_nodeManager = NULL;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

NodeManager::NodeManager()
{
	m_nextClientID = 0;
}

NodeManager::~NodeManager()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Database manipulation

/**
	@brief Allocate a new client ID
 */
clientID NodeManager::AllocateClient(string hostname)
{
	clientID id;

	m_mutex.lock();
		id = m_nextClientID ++;
		m_workingCopies[id].SetInfo(hostname, id);
	m_mutex.unlock();

	return id;
}

/**
	@brief Delete any toolchains registered to a node
 */
void NodeManager::RemoveClient(clientID id)
{
	m_mutex.lock();

		auto chains = m_toolchainsByNode[id];
		for(auto x : chains)
			delete x.second;

		m_toolchainsByNode.erase(id);

		for(auto x : m_nodesByLanguage)
			m_nodesByLanguage[x.first].erase(id);

		for(auto x : m_nodesByCompiler)
			m_nodesByCompiler[x.first].erase(id);

		m_workingCopies.erase(id);

	m_mutex.unlock();

	//Toolchains may have changed, recompute
	RecomputeCompilerHashes();
}

/**
	@brief Add a toolchain to a node

	moreComing is a hint from the caller that there are more additions pending in the immediate future.
	If set, we defer the recomputation until it's done.
 */
void NodeManager::AddToolchain(clientID id, Toolchain* chain, bool moreComing)
{
	m_mutex.lock();

		string hash = chain->GetHash();
		m_toolchainsByNode[id][hash] = chain;
		auto langs = chain->GetSupportedLanguages();
		auto triplets = chain->GetTargetTriplets();
		for(auto l : langs)
			for(auto t : triplets)
				m_nodesByLanguage[larch(l, t)].emplace(id);
		m_nodesByCompiler[hash].emplace(id);

	m_mutex.unlock();

	//Toolchains may have changed, recompute
	if(!moreComing)
		RecomputeCompilerHashes();
}

/**
	@brief Calculate the hash corresponding to each toolchain we know about
 */
void NodeManager::RecomputeCompilerHashes()
{
	m_mutex.lock();

		LogDebug("Recomputing compiler hashes\n");

		//Make a list of every architecture we know about.
		//The list of toolchains is architecture specific so we need to know what the options are.
		LogDebug("    Enumerating language-architecture tuples\n");
		unordered_set<larch> larches;
		for(auto it : m_nodesByLanguage)
			larches.emplace(it.first);

		//For each architecture, find the best toolchain available
		for(auto x : larches)
		{
			//Initial toolchain: garbage
			int type = Toolchain::TOOLCHAIN_NULL;
			int major = 0;
			int minor = 0;
			int patch = 0;
			string bestHash = "";
			string bestVersionString = "";	//mostly for debug printing

			//Loop over all toolchains we have (TODO: only per language? index this somehow?)
			for(auto it : m_nodesByCompiler)
			{				
				//Skip if no nodes for this hash (should never happen)
				//TODO: log warning if it does?
				if(it.second.empty())
					continue;
				
				//Find a client that has this toolchain installed
				clientID id = *it.second.begin();

				//Look up the toolchain object for that client
				string hash = it.first;
				Toolchain* tool = m_toolchainsByNode[id][hash];

				//If we can't compile our platform with this toolchain it's of no value to us :(
				if(!tool->IsLanguageSupported(x.first))
					continue;
				if(!tool->IsArchitectureSupported(x.second))
					continue;

				//See if it's any better than what we have now
				//Worse tool is always worse.
				//Better tool is always better.
				//If same tool, better version wins
				int ttype = tool->GetType();
				int tmaj = tool->GetMajorVersion();
				int tmin = tool->GetMinorVersion();
				int tpatch = tool->GetPatchVersion();
				if(ttype < type)
					continue;
				else if(ttype == type)
				{
					if(tmaj < major)
						continue;
					else if(tmaj == major)
					{
						if(tmin < minor)
							continue;
						else if(tmin == minor)
						{
							if(tpatch < patch)
								continue;
						}
					}
				}

				type = ttype;
				major = tmaj;
				minor = tmin;
				patch = tpatch;
				bestHash = hash;
				bestVersionString = tool->GetVersionString();
			}

			//No toolchain found? Guess we can't compile anything for this platform
			if(type == Toolchain::TOOLCHAIN_NULL)
			{
				LogDebug("Best toolchain for %15s on %20s is: [none found]\n",
					Toolchain::LangToString(x.first).c_str(), x.second.c_str());
			}

			//Found something
			else
			{
				LogDebug("Best toolchain for %15s on %20s is %s\n",
					Toolchain::LangToString(x.first).c_str(),
					x.second.c_str(),
					bestVersionString.c_str());
			}
			
		}

	m_mutex.unlock();
}
