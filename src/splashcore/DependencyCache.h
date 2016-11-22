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

#ifndef DependencyCache_h
#define DependencyCache_h

class CachedDependencies
{
public:

	//True if the scan is successful
	bool m_ok;

	//stdout of the scan
	std::string m_stdout;

	//The dependencies
	std::set<std::string> m_deps;

	//Map of (fname, hash) tuples
	std::map<std::string, std::string>	m_filedeps;

	//Flags related to libraries that we discovered
	std::set<BuildFlag> m_libflags;
};

/**
	@brief A cache of dependency-scan results
 */
class DependencyCache
{
public:
	DependencyCache();
	virtual ~DependencyCache();

	std::string GetHash(std::string fname, std::string triplet, const std::set<BuildFlag>& flags);

	void AddEntry(std::string hash, CachedDependencies& deps);

	/**
		@brief Check if a given object is in the cache
	 */
	bool IsCached(std::string hash)
	{ return m_cache.find(hash) != m_cache.end(); }

	/**
		@brief Get the cache results for a given hash index
	 */
	const CachedDependencies& GetCachedDependencies(std::string hash)
	{ return m_cache[hash]; }

protected:
	std::map<std::string, CachedDependencies> m_cache;
};

#endif

