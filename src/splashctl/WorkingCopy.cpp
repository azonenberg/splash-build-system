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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

WorkingCopy::WorkingCopy()
{
}

WorkingCopy::~WorkingCopy()
{
}

/**
	@brief Save info about the working copy, mostly for debug logging
 */
void WorkingCopy::SetInfo(string hostname, clientID id)
{
	m_hostname = hostname;
	m_id = id;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Directory structure manipulation

/**
	@brief Updates a file in the current working copy

	@param path		Relative path of the file
	@param hash		SHA-256 hash of the file
 */
void WorkingCopy::UpdateFile(string path, string hash)
{
	//If the file is a build.yml, process it
	if(GetBasenameOfFile(path) == "build.yml")
		m_graph.UpdateScript(path, hash);

	m_fileMap[path] = hash;
}

/**
	@brief Removes a file in the current working copy

	@param path		Relative path of the file
 */
void WorkingCopy::RemoveFile(string path)
{
	//If the file is a build.yml, process it
	if(GetBasenameOfFile(path) == "build.yml")
		m_graph.RemoveScript(path);

	m_fileMap.erase(path);
}
