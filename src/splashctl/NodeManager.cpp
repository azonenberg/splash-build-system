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
			delete x;

		m_toolchainsByNode.erase(id);

		for(auto x : m_nodesByLanguage)
			x.second.erase(id);

		for(auto x : m_nodesByCompiler)
			x.second.erase(id);

		m_workingCopies.erase(id);

	m_mutex.unlock();
}

/**
	@brief Add a toolchain to a node
 */
void NodeManager::AddToolchain(clientID id, Toolchain* chain)
{
	m_mutex.lock();

		string hash = chain->GetHash();
		m_toolchainsByNode[id].emplace(chain);
		auto langs = chain->GetSupportedLanguages();
		auto triplets = chain->GetTargetTriplets();
		for(auto l : langs)
			for(auto t : triplets)
				m_nodesByLanguage[larch(l, t)].emplace(id);
		m_nodesByCompiler[hash].emplace(id);

	m_mutex.unlock();
}
