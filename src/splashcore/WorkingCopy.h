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

#ifndef WorkingCopy_h
#define WorkingCopy_h

//unique ID for a client
typedef std::string clientID;

/**
	@brief An individual client's working copy
 */
class WorkingCopy
{
public:
	WorkingCopy(std::string hostname, clientID id);
	virtual ~WorkingCopy();

	void UpdateFile(std::string path, std::string hash, bool body, bool config);
	void RemoveFile(std::string path);

	std::string GetFileHash(std::string path);
	bool HasFile(std::string path);

	void RefreshToolchains();

	std::string GetHostname()
	{ return m_hostname; }

	BuildGraph& GetGraph()
	{ return m_graph; }

	void AddClient(int type);
	void RemoveClient(int type);

	/**
		@brief Get the number of clients we have of each type
	 */
	int GetClientCount(ClientHello::ClientType type)
	{
		if(type >= ClientHello::CLIENT_COUNT)
			return 0;
		return m_haveClients[type];
	}

protected:

	//Type of clients we have connected
	int m_haveClients[ClientHello::CLIENT_COUNT];

	//Our mutex (need to be able to lock when already locked)
	std::recursive_mutex m_mutex;

	/**
		@brief Map of relative paths to hashes

		Object/executable files TODO
	 */
	std::map<std::string, std::string> m_fileMap;

	//Info about this working copy
	std::string m_hostname;
	clientID m_id;
	
	//The parsed build graph for this working copy
	BuildGraph m_graph;
};

#endif

