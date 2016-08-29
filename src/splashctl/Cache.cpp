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

Cache* g_cache = NULL;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

Cache::Cache()
{
	LogVerbose("Initializing cache subsystem...\n");
	
	//TODO: get cache path from command line arg or something?
	//Hard-coding the path prevents more than one splashctl instance from running on a given server.
	//This may or may not be what we want. If we DO want to limit to one instance, we need a way for multiple distinct
	//projects to share the splashctl/splashbuild back end.
	string home = getenv("HOME");
	if(!DoesDirectoryExist(home))
		LogFatal("home dir does not exist\n");
	m_cachePath = home + "/.splash/splashctl-cache";
	
	//If the cache directory does not exist, create it
	if(!DoesDirectoryExist(m_cachePath))
	{
		LogDebug("Cache directory does not exist, creating it\n");
		MakeDirectoryRecursive(m_cachePath, 0600);
	}
	
	//It exists, load existing entries
	else
	{
		LogDebug("Cache directory exists, loading items\n");
		
		//TODO: Read cache entries from disk
	}
}

Cache::~Cache()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Status queries

/**
	@brief Determine if a particular object is in the cache.
	
	@param id		Object ID hash
 */
bool Cache::IsCached(string id)
{
	bool found = false;
	m_mutex.lock();
		if(m_cacheIndex.find(id) != m_cacheIndex.end())
			found = true;
	m_mutex.unlock();
	
	return found;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cache manipulation

/**
	@brief Adds a file to the cache
	
	@param basename			Name of the file without directory information
	@param id				Object ID hash
	@param hash				SHA-256 sum of the file
	@param data				Content of the file
	@param len				Length of the file
 */
void Cache::AddFile(string basename, string id, string hash, const unsigned char* data, uint64_t len)
{
	m_mutex.lock();
	
		//Create the directory. If it already exists, delete whatever junk was in there
		string dirname = m_cachePath + "/" + id.substr(0, 2) + "/" + id;
		if(DoesDirectoryExist(dirname))
		{
			LogWarning("Cache directory %s already exists but was not loaded, removing dead files\n", hash.c_str());
			ShellCommand(string("rm -I ") + dirname + "/*");
		}
		else
			MakeDirectoryRecursive(dirname, 0600);
			
		//Write the file data to it
		string fname = dirname + "/" + basename;
		FILE* fp = fopen(fname.c_str(), "wb");
		if(!fp)
		{
			m_mutex.unlock();
			LogWarning("Couldn't create cache file %s\n", fname.c_str());
			return;
		}
		if(len != fwrite(data, 1, len, fp))
		{
			fclose(fp);
			m_mutex.unlock();
			LogWarning("Couldn't write cache file %s\n", fname.c_str());
			return;
		}
		fclose(fp);
		
		//Write the hash
		fname = dirname + "/.hash";
		fp = fopen(fname.c_str(), "wb");
		if(!fp)
		{
			m_mutex.unlock();
			LogWarning("Couldn't create cache file %s\n", fname.c_str());
			return;
		}
		if(64 != fwrite(hash.c_str(), 1, 64, fp))
		{
			fclose(fp);
			m_mutex.unlock();
			LogWarning("Couldn't write cache file %s\n", fname.c_str());
			return;
		}
		fclose(fp);
		
		//TODO: write the atime file?
	
	m_mutex.unlock();
}
