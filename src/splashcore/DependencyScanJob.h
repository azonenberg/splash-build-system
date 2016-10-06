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

#ifndef DependencyScanJob_h
#define DependencyScanJob_h

/**
	@brief A request to perform a dependency scan
 */
class DependencyScanJob : public Job
{
public:

	DependencyScanJob(
		std::string path,
		WorkingCopy* wc,
		std::string toolchainHash,
		std::string arch,
		std::set<BuildFlag> flags);

protected:
	//must delete via refcounter
	virtual ~DependencyScanJob();

public:

	std::string GetPath()
	{ return m_sourcePath; }

	WorkingCopy* GetWorkingCopy()
	{ return m_workingCopy; }

	const std::set<BuildFlag>& GetFlags()
	{ return m_flags; }

	std::string GetArch()
	{ return m_arch; }

	void AddDependency(std::string fname, std::string hash)
	{ m_output[fname] = hash; }

	/**
		@brief Gets a read-only pointer to the scan results

		Only valid if the job is on "completed" state
	 */
	const std::map<std::string, std::string>& GetOutput()
	{ return m_output; }

protected:

	/// @brief Path of the input file
	std::string m_sourcePath;

	/// @brief The working copy we're attached to
	WorkingCopy* m_workingCopy;

	/// @brief Flags this build requires
	std::set<BuildFlag> m_flags;

	/// @brief Architecture we're scanning for
	std::string m_arch;

	/**
		@brief OUTPUT of the scan

		Map of <fname, hash>.
		The hash is needed b/c we may have system headers, etc
	 */
	std::map<std::string, std::string> m_output;
};

#endif
