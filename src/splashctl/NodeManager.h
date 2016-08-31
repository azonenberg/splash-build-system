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

#ifndef NodeManager_h
#define NodeManager_h

//set of toolchains
typedef std::unordered_set<Toolchain*> vtool;

//set of node IDs
typedef std::unordered_set<clientID> vnode;

//(language, target arch) tuple
typedef std::pair<Toolchain::Language, std::string> larch;

/**
	@brief List of nodes connected to the server, and various info about them

	All functions except constructor/destructor are thread safe
 */
class NodeManager
{
public:
	NodeManager();
	virtual ~NodeManager();

	clientID AllocateClient(std::string hostname);
	void RemoveClient(clientID id);

	void AddToolchain(clientID id, Toolchain* chain);

	WorkingCopy& GetWorkingCopy(clientID id)
	{ return m_workingCopies[id]; }

protected:

	//Mutex to control access to all node lists
	std::mutex m_mutex;

	//List of compilers available on each node
	//This is the authoritative pointer to nodes
	std::map<clientID, vtool> m_toolchainsByNode;

	//List of nodes with any compiler for a given language and target architecture
	std::map<larch, vnode> m_nodesByLanguage;

	//List of nodes with a specific compiler (by hash)
	std::map<std::string, vnode> m_nodesByCompiler;

	//The working copies of the repository for each client
	std::map<clientID, WorkingCopy> m_workingCopies;

	clientID m_nextClientID;
};

extern NodeManager* g_nodeManager;

#endif