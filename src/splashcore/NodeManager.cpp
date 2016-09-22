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

NodeManager* g_nodeManager = NULL;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

NodeManager::NodeManager()
{
}

NodeManager::~NodeManager()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Mutex operations


/**
	@brief Locks the node manager's mutex. This prevents any database manipulation by other threads.

	It is critical to lock the mute whenever querying a specific toolchain object.
	Compute nodes can come and go at any time while the mutex is unlocked; pointers may become invalid at any time.
	Do not retain any node pointers after unlocking the mutex.
 */
void NodeManager::lock()
{
	m_mutex.lock();
}

/**
	@brief Unlocks the node manager's mutex.
 */
void NodeManager::unlock()
{
	m_mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Database manipulation

/**
	@brief Allocate a new client ID
 */
void NodeManager::AllocateClient(string hostname, string uuid)
{
	lock_guard<recursive_mutex> lock(m_mutex);
	m_workingCopies[uuid] = new WorkingCopy;
	m_workingCopies[uuid]->SetInfo(hostname, uuid);
}

/**
	@brief Delete any state registered to a node
 */
void NodeManager::RemoveClient(clientID id)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	//Cancel all pending jobs for that node
	g_scheduler->RemoveNode(id);

	auto chains = m_toolchainsByNode[id];
	for(auto x : chains)
		delete x.second;

	m_toolchainsByNode.erase(id);

	for(auto x : m_nodesByLanguage)
		m_nodesByLanguage[x.first].erase(id);

	for(auto x : m_nodesByCompiler)
		m_nodesByCompiler[x.first].erase(id);

	delete m_workingCopies[id];
	m_workingCopies.erase(id);

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
	lock_guard<recursive_mutex> lock(m_mutex);

	string hash = chain->GetHash();
	m_toolchainsByNode[id][hash] = chain;
	auto langs = chain->GetSupportedLanguages();
	auto triplets = chain->GetTargetTriplets();
	for(auto l : langs)
		for(auto t : triplets)
			m_nodesByLanguage[larch(l, t)].emplace(id);
	m_nodesByCompiler[hash].emplace(id);

	//Toolchains may have changed, recompute
	if(!moreComing)
		RecomputeCompilerHashes();
}

/**
	@brief Calculate the hash corresponding to each toolchain we know about

	DOES NOT lock the mutex; must be called only after it's locked.
 */
void NodeManager::RecomputeCompilerHashes()
{
	LogDebug("Recomputing compiler hashes\n");

	//Make a list of every language/architecture pair we know about.
	//The list of languages is architecture specific so we need to know what the options are.
	//It's stupid to wasted time looking for a Verilog compiler for mips-elf.
	set<larch> larches;
	for(auto it : m_nodesByLanguage)
		larches.emplace(it.first);

	//Map of toolchain IDs to hashes
	map<carch, string> currentToolchains;

	//For each toolchain we know about, generate the names
	for(auto x : m_nodesByCompiler)
	{
		string hash = x.first;
		Toolchain* tool = GetAnyToolchainForHash(hash);
		if(tool == NULL)
			continue;

		//Generate the names for each arch/triplet combination
		set<string> names;
		tool->GetCompilerNames(names);
		auto triplets = tool->GetTargetTriplets();
		for(auto arch : triplets)
		{
			for(auto name : names)
			{
				//If nobody has claimed this name/arch yet, take it
				carch c(name, arch);
				if(currentToolchains.find(c) == currentToolchains.end())
				{
					currentToolchains[c] = hash;
					continue;
				}

				//If another node took the tuple already, keep the higher version
				Toolchain* otherTool = GetAnyToolchainForHash(currentToolchains[c]);
				if(tool->CompareVersion(otherTool) >= 0)
					currentToolchains[c] = hash;
			}
		}

	}

	//For each language/architecture, find the best toolchain available
	for(auto x : larches)
	{
		//Initial toolchain: nonexistent
		Toolchain* bestToolchain = NULL;
		string bestHash = "";

		//Loop over all toolchains we have (TODO: only per language? index this somehow?)
		for(auto it : m_nodesByCompiler)
		{
			//Look up the toolchain object for that client
			string hash = it.first;
			Toolchain* tool = GetAnyToolchainForHash(hash);
			if(tool == NULL)
				continue;

			//If we can't compile our platform with this toolchain it's of no value to us :(
			if(!tool->IsLanguageSupported(x.first))
				continue;
			if(!tool->IsArchitectureSupported(x.second))
				continue;

			//See if it's any better than what we have now. Skip it if not
			if((bestToolchain != NULL) && (tool->CompareVersionAndType(bestToolchain) < 0) )
				continue;

			bestToolchain = tool;
			bestHash = hash;
		}

		//Register the generic toolchains
		if(bestToolchain != NULL)
		{
			string lang = MakeStringLowercase(Toolchain::LangToString(x.first));
			currentToolchains[carch(lang + "/generic", x.second)] = bestHash;
		}
	}

	//Update the global toolchain list
	bool changed = (m_toolchainsByName != currentToolchains);
	m_toolchainsByName = currentToolchains;

	/*
	//DEBUG: Print the final mapping
	for(auto it : m_toolchainsByName)
	{
		carch c = it.first;
		string hash = it.second;
		Toolchain* tool = GetAnyToolchainForHash(hash);

		LogDebug("%25s for %20s is %25s (%s)\n",
			c.first.c_str(), c.second.c_str(), tool->GetVersionString().c_str(), hash.c_str());
	}
	*/

	//FIXME: do something more efficient based on what changed
	//For now, just re-run every build script in every working copy.
	if(changed)
	{
		for(auto it : m_workingCopies)
			it.second->RefreshToolchains();
	}
}

/**
	@brief Get a pointer to some node's toolchain for the specified hash.

	The exact choice of node is unpredictable and should not be relied on.

	MUST be called while m_mutex is locked. Do not use the returned pointer once the mutex is unlocked.
 */
Toolchain* NodeManager::GetAnyToolchainForHash(string hash)
{
	//If we couldn't find any instances, then complain
	if(m_nodesByCompiler.find(hash) == m_nodesByCompiler.end())
		return NULL;
	if(m_nodesByCompiler[hash].empty())
		return NULL;

	//Find a client that has this toolchain installed
	clientID id = *m_nodesByCompiler[hash].begin();

	//Look up the toolchain object for that client
	return m_toolchainsByNode[id][hash];
}

/**
	@brief Get a pointer to some node's toolchain for the specified toolchain name and target architecture.

	The exact choice of node is unpredictable and should not be relied on.

	MUST be called while m_mutex is locked. Do not use the returned pointer once the mutex is unlocked.
 */
Toolchain* NodeManager::GetAnyToolchainForName(string arch, string name)
{
	lock_guard<recursive_mutex> lock(m_mutex);
	return GetAnyToolchainForHash(GetToolchainHash(arch, name));
}

/**
	@brief Gets the current hash of the requested toolchain, or the empty string if it doesn't exist.
 */
string NodeManager::GetToolchainHash(string arch, string name)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	//If we do not have such a toolchain, give up
	carch c(name, arch);
	if(m_toolchainsByName.find(c) == m_toolchainsByName.end())
		return "";
	return m_toolchainsByName[c];
}

/**
	@brief Get the ID of the "golden node" for the specified toolchain hash.

	This functionality is not fully implemented.

	MUST be called while m_mutex is locked. Do not use the returned value once the mutex is unlocked.
 */
clientID NodeManager::GetGoldenNodeForToolchain(string hash)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	//If we know about the toolchain, but nobody has it, give up
	if(m_nodesByCompiler.find(hash) == m_nodesByCompiler.end())
		return 0;
	auto nodes = m_nodesByCompiler[hash];
	if(nodes.empty())
		return 0;

	//If we have only one node, return it.
	if(nodes.size() == 1)
		return *nodes.begin();

	//If more than one, print a warning and return the first
	//TODO: Pick golden node based on explicit config
	else
	{
		LogWarning("NodeManager::GetGoldenNodeForToolchain() does not have golden config yet\n");
		return *nodes.begin();
	}
}
