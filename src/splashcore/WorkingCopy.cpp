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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

WorkingCopy::WorkingCopy(string hostname, clientID id)
	: m_hostname(hostname)
	, m_id(id)
	, m_graph(this)
{
	for(int i=0; i<ClientHello::CLIENT_COUNT; i++)
		m_haveClients[i] = 0;

	//LogDebug("creating working copy %p for hostname %s, uuid %s\n", this, hostname.c_str(), id.c_str());

	//TODO: How do we persist all of the state for output files etc?
}

WorkingCopy::~WorkingCopy()
{
	//LogDebug("deleting working copy %p\n", this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

/**
	@brief Add a client from this working copy
 */
void WorkingCopy::AddClient(int type)
{
	if( (type >= ClientHello::CLIENT_COUNT) || (type < 0) )
		return;

	lock_guard<recursive_mutex> lock(m_mutex);
	m_haveClients[type] ++;
}

/**
	@brief Remove a client from this working copy
 */
void WorkingCopy::RemoveClient(int type)
{
	if( (type >= ClientHello::CLIENT_COUNT) || (type < 0) )
		return;

	lock_guard<recursive_mutex> lock(m_mutex);
	m_haveClients[type] --;
}

/**
	@brief See if we have a file with a specific path
 */
bool WorkingCopy::HasFile(string path)
{
	lock_guard<recursive_mutex> lock(m_mutex);
	return (m_fileMap.find(path) != m_fileMap.end());
}

/**
	@brief Gets the hash of a specific file, if we have it
 */
string WorkingCopy::GetFileHash(string path)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	if(m_fileMap.find(path) != m_fileMap.end())
		return m_fileMap[path];

	else
	{
		LogWarning("WorkingCopy: asked for nonexistent file %s\n", path.c_str());
		return "";
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Directory structure manipulation

/**
	@brief Updates a file in the current working copy

	@param path			Relative path of the file
	@param hash			SHA-256 hash of the file
	@param body			True if we should re-scan the body (ignored if not a build script)
	@param config		For a build script: True if we should re-scan the config section.
						For a source file:  True if we should re-scan dependent build scripts.
 */
void WorkingCopy::UpdateFile(string path, string hash, bool body, bool config)
{
	//LogDebug("WorkingCopy::UpdateFile %s\n", path.c_str());

	string buildscript = GetDirOfFile(path) + "/build.yml";
	bool has_script = HasFile(buildscript);

	//See if we created a new file
	//bool created = (m_fileMap.find(path) == m_fileMap.end());

	//Update our records for the new file before doing anything else
	//since future processing may depend on this file existing.
	//Note that we do *not* want the entire working copy locked after the path update
	//since dependency scanning might have to read it
	m_mutex.lock();
	m_fileMap[path] = hash;
	m_mutex.unlock();

	//If the file is a build.yml, process it
	bool is_script = (GetBasenameOfFile(path) == "build.yml");
	if(is_script)
		m_graph.UpdateScript(path, hash, body, config);

	//If we have a build.yml in the directory, re-run its targets even if we didn't make the file new
	//(since we may have added an include statement, etc
	if(has_script && !is_script && config)
		m_graph.UpdateScript(buildscript, m_fileMap[buildscript], true, false);

	//TODO: Have a list of nodes that depend on each file and re-create them?
	//The current "rerun current dir" stuff only works if we do not allow pulling source from another directory!
}

/**
	@brief Removes a file in the current working copy

	@param path		Relative path of the file
 */
void WorkingCopy::RemoveFile(string path)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	//If the file is a build.yml, process it
	if(GetBasenameOfFile(path) == "build.yml")
		m_graph.RemoveScript(path);

	m_fileMap.erase(path);
}

/**
	@brief Called when available toolchains have changed

	For now, just re-run all build scripts
 */
void WorkingCopy::RefreshToolchains()
{
	lock_guard<recursive_mutex> lock(m_mutex);

	//Find build scripts
	vector<string> paths;
	for(auto it : m_fileMap)
	{
		string fname = it.first;
		if(GetBasenameOfFile(fname) == "build.yml")
			paths.push_back(fname);
	}

	//Sort the paths lexically
	//This way we do root dirs first so that we don't waste time doing multiple refreshes
	sort(paths.begin(), paths.end());

	//And evaluate them in that order
	for(auto p : paths)
	{
		//LogDebug("Re-evaluating build script %s\n", p.c_str());
		m_graph.UpdateScript(p, m_fileMap[p], true, true);
	}
}
