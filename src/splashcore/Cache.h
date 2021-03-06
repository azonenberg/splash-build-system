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

#ifndef cache_h
#define cache_h

/*
	@brief Set of hashes for source/object files we have in the cache (need to read at app startup)

	Objects are identified by "object ID" SHA-256 hashes. What goes into the hash varies depending on the type of the
	object being identified:
		* For source/input files the object ID is simply the hash of the file contents
		* For generated files, the object ID is the hash of all inputs, the compiler, flags, etc.

	Directory structure:
	$CACHE/
		xx/				first octet of hash, as hex
			hash/		hash of file object (may not actually be the hash of the file, includes flags etc)
				data	the file itself
				hash	sha256 of the file itself (for load-time integrity checking)
				atime	last-accessed time of the file
						We don't use filesystem atime as that's way too easy to set by accident

	All functions (aside from constructor/destructor) are thread safe and include locking where necessary.

	TODO: provide some way to "pin" actively referenced items in the cache so we don't evict them, even if not
	recently changed
 */
class Cache
{
public:
	Cache(std::string cachename);
	virtual ~Cache();

	NodeInfo::NodeState GetState(std::string id);

	bool IsCached(std::string id);
	bool IsFailed(std::string id);
	bool ValidateCacheEntry(std::string id);

	void AddFile(std::string basename, std::string id, std::string hash, std::string data, std::string log = "");
	void AddFailedFile(std::string basename, std::string id, std::string log);

	std::string ReadCachedFile(std::string id);
	bool ReadCachedLog(std::string id, std::string& log);

	std::string GetContentHash(std::string id);

protected:

	//TODO: Garbage collection (the cache grows without bound for now!)

	std::string GetStoragePath(std::string id);

	//Mutex to control access to all global cache state
	std::recursive_mutex m_mutex;

	//Set of hashes we have in the cache that failed to build
	std::unordered_set<std::string> m_cacheFails;

	//Map of ID hashes to content hashes
	std::map<std::string, std::string> m_contentHashes;

	//TODO: map<string, time_t>		mapping hashes to last-used times

	//Directory that the cache is stored in
	std::string m_cachePath;

	//TODO: total disk space used by cache items
};

extern Cache* g_cache;

#endif
