/***********************************************************************************************************************
*                                                                                                                      *
* SPLASH build system v0.2                                                                                             *
*                                                                                                                      *
* Copyright (c) 2016-2017 Andrew D. Zonenberg                                                                          *
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

#ifndef BuildGraphNode_h
#define BuildGraphNode_h

/**
	@brief A single node in the build graph
 */
class BuildGraphNode
{
public:
	BuildGraphNode();

	BuildGraphNode(
		BuildGraph* graph,
		BuildFlag::FlagUsage usage,
		std::string path,
		std::string hash
		);

	BuildGraphNode(
		BuildGraph* graph,
		BuildFlag::FlagUsage usage,
		std::string toolchain,
		std::string arch,
		std::string name,
		std::string scriptpath,
		std::string path,
		std::set<BuildFlag> flags
		);

	BuildGraphNode(
		BuildGraph* graph,
		BuildFlag::FlagUsage usage,
		std::string toolchain,
		std::string arch,
		std::string config,
		std::string name,
		std::string scriptpath,
		std::string path,
		YAML::Node& node);

	virtual ~BuildGraphNode();

	void StartFinalization();
	void Finalize();

	/**
		@brief Get the graph that the node is attached to

		No mutexing needed as this is static after object creation
	 */
	BuildGraph* GetGraph()
	{ return m_graph; }

	/**
		@brief Get the hash of the node

		No mutexing needed as this is static after object creation
	 */
	std::string GetHash()
	{ return m_hash; }

	/**
		@brief Get the name of the node

		No mutexing needed as this is static after object creation
	 */
	std::string GetName()
	{ return m_name; }


	/**
		@brief Get the script of the node

		No mutexing needed as this is static after object creation
	 */
	std::string GetScript()
	{ return m_script; }

	/**
		@brief Get the toolchain name of the node

		No mutexing needed as this is static after object creation
	 */
	std::string GetToolchain()
	{ return m_toolchain; }

	/**
		@brief Get the toolchain hash of the node

		No mutexing needed as this is static after object creation
	 */
	std::string GetToolchainHash()
	{ return m_toolchainHash; }

	/**
		@brief Get the architecture of the node

		No mutexing needed as this is static after object creation
	 */
	std::string GetArch()
	{ return m_arch; }

	/**
		@brief Get the dependencies of the node

		No mutexing needed as this is static after object creation
	 */
	const std::set<std::string>& GetDependencies() const
	{ return m_dependencies; }

	/**
		@brief Get the configuration of the node

		No mutexing needed as this is static after object creation
	 */
	std::string GetConfig()
	{ return m_config; }

	void GetFlagsForUseAt(
		BuildFlag::FlagUsage when,
		std::set<BuildFlag>& flags);

	/**
		@brief Gets the path of our node (relative to the working copy)

		No mutexing needed as this is static after object creation
	 */
	std::string GetFilePath()
	{ return m_path; }

	/**
		@brief Gets the path of our node (relative to the working copy)

		No mutexing needed as this is static after object creation
	 */
	const std::set<std::string>& GetSources()
	{ return m_sources; }

	/**
		@brief Gets our cache state

		No mutexing needed as cache handles this for us
	 */
	NodeInfo::NodeState GetOutputState()
	{ return g_cache->GetState(m_hash); }

	/**
		@brief Checks if we've been finalized yet

		No mutexing since reading a bool should be atomic (and it only ever goes one way, from false to true)
	 */
	bool IsFinalized()
	{ return m_finalized; }

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Actual building

	virtual Job* Build(Job::Priority prio = Job::PRIO_NORMAL);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Debug helpers

	void PrintInfo(int indentLevel = 0);
	std::string GetIndent(int level);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Garbage collection during graph rebuilds

	void SetRef();
	void SetUnref();
	bool IsReferenced();

protected:

	void SetInvalidInput(std::string errors);

	//Load source file nodes, creating new ones as needed
	void LoadSourceFileNodes(
		YAML::Node& node,
		const std::string& scriptpath,
		const std::string& name,
		const std::string& path,
		std::set<BuildGraphNode*>& sourcenodes);

	//Update hash for a standard target without anything funny going on
	void UpdateHash_DefaultTarget();

	virtual void DoFinalize() =0;
	virtual void DoStartFinalization();

	//Our mutex (need to be able to lock when already locked)
	std::recursive_mutex m_mutex;

	/// @brief Indicates if the node is referenced
	bool m_ref;

	/// @brief Pointer to our parent graph
	BuildGraph* m_graph;

	/// @brief The toolchain this node is built with (may be empty string if a source file, etc)
	std::string m_toolchain;

	/// @brief The hash of the toolchain we're using (must be set in constructor; immutable)
	std::string m_toolchainHash;

	/**
		@brief The hash of this node.

		Must be set to something, even a temporary value, in the constructor.
		Once Finalize() is called, this value is immutable; further changes are not allowed.
		Changing during DoFinalize() is legal.
	 */
	std::string m_hash;

	/// @brief Architecture of this node, or "generic" if independent (source file etc)
	std::string m_arch;

	/// @brief Configuration of this node, or "generic" if independent
	std::string m_config;

	/// @brief Human-readable name of this node (for debug messages)
	std::string m_name;

	/// @brief Path to the build script this node was declared in (for debug messages and relative paths)
	std::string m_script;

	/// @brief Path to the file this node creates (or the input file, for source nodes)
	std::string m_path;

	/// @brief Flags applied to the node at any step (note that target nodes may have flags for earlier stages too)
	std::set<BuildFlag> m_flags;

	/**
		@brief Set of named files we depend on (source files, object files, etc)

		The dependency list consists of files used, directly or otherwise, by this build step only. For example:
		* An object file depends on the source and headers (including headers included indirectly).
		* An executable depends on objects, but not sources.
	 */
	std::set<std::string> m_dependencies;

	/**
		@brief Set of named files we feed directly to the compiler.

		This should always be a strict subset of m_dependencies.
	 */
	std::set<std::string> m_sources;

	/**
		@brief The type of build operation we're doing (important for us to use the correct flags)
	 */
	BuildFlag::FlagUsage m_usage;

	/// @brief The job we currently have pending to build us
	Job* m_job;

	/// @brief Indicates that this node has started the finalization process
	bool m_finalizationStarted;

	/// @brief Indicates that this node has been finalized.
	bool m_finalized;

private:
	/**
		@brief Indicates that this node is in an "error" state and cannot be built.

		This is used for things like "missing input files" etc.
	 */
	bool m_invalidInput;
};

#endif
