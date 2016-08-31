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
		for(unsigned int i=0; i<256; i++)
		{
			//If the cache isn't very full we might not have entries in every bucket
			char hex[3];
			snprintf(hex, sizeof(hex), "%02x", i);
			string dirname = m_cachePath + "/" + hex;
			if(!DoesDirectoryExist(dirname))
				continue;

			//It exists, load whatever is in it
			vector<string> subdirs;
			FindSubdirs(dirname, subdirs);
			for(auto dir : subdirs)
			{
				//The object ID is what goes in our table, not the file hash
				string oid = GetBasenameOfFile(dir);
				if(!ValidateCacheEntry(oid))
					continue;

				//Cache entry is valid, add to the cache table
				m_cacheIndex.emplace(oid);
			}
		}

		LogVerbose("    %d cache entries loaded\n", (int)m_cacheIndex.size());
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

/**
	@brief Stronger form of IsCached(). Checks if the internal hash is consistent

	@param id		Object ID hash
 */
bool Cache::ValidateCacheEntry(string id)
{
	m_mutex.lock();

	//If the directory doesn't exist, obviously we have nothing useful there
	string dir = GetStoragePath(id);
	if(!DoesDirectoryExist(dir))
	{
		m_mutex.unlock();
		return false;
	}

	//If we're missing the file or hash, can't possibly be valid
	string fpath = dir + "/data";
	string hpath = dir + "/hash";
	if( !DoesFileExist(fpath) || !DoesFileExist(hpath) )
	{
		m_mutex.unlock();
		return false;
	}

	//Get the expected hash and compare to the real one
	//If it's invalid, just get rid of the junk
	string expected = GetFileContents(hpath);
	string found = sha256_file(fpath);
	if(expected != found)
	{
		LogWarning("Cache directory %s is corrupted (hash match failed)\n", id.c_str());
		ShellCommand(string("rm -I ") + dir + "/*");
		ShellCommand(string("rm -r ") + dir);
		m_mutex.unlock();
		return false;
	}

	m_mutex.unlock();
	return true;
}

/**
	@brief Gets the cache path used for storing a particular object
 */
string Cache::GetStoragePath(string id)
{
	return m_cachePath + "/" + id.substr(0, 2) + "/" + id;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cache manipulation

/**
	@brief Reads a file from the cache
 */
string Cache::ReadCachedFile(string id)
{
	m_mutex.lock();

		//Sanity check
		if(m_cacheIndex.find(id) == m_cacheIndex.end())
		{
			m_mutex.unlock();
			LogError("WARNING: requested file %s is not in cache\n", id.c_str());
			return "";
		}

		//Read the file
		string ret = GetFileContents(GetStoragePath(id));

	m_mutex.unlock();

	return ret;
}

/**
	@brief Adds a file to the cache

	@param basename			Name of the file without directory information
	@param id				Object ID hash
	@param hash				SHA-256 sum of the file
	@param data				Content of the file
	@param len				Length of the file
 */
void Cache::AddFile(string /*basename*/, string id, string hash, const unsigned char* data, uint64_t len)
{
	m_mutex.lock();

		//Create the directory. If it already exists, delete whatever junk was in there
		string dirname = GetStoragePath(id);
		if(DoesDirectoryExist(dirname))
		{
			LogWarning("Cache directory %s already exists but was not loaded, removing dead files\n", hash.c_str());
			ShellCommand(string("rm -I ") + dirname + "/*");
		}
		else
			MakeDirectoryRecursive(dirname, 0600);

		//Write the file data to it
		string fname = dirname + "/data";
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
		fname = dirname + "/hash";
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

		//Remember that we have this file cached
		m_cacheIndex.emplace(hash);

		//TODO: add this file's size to the cache, if we went over the cap delete the LRU file

	m_mutex.unlock();
}
