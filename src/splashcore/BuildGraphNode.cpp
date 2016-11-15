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

/**
	@brief Default constructor (for non-targets)

	TODO: how do we use this? Disallow?
 */
BuildGraphNode::BuildGraphNode()
	: m_ref(false)
	, m_job(NULL)
	, m_finalized(false)
{
}

/**
	@brief Constructor for nodes which are source files
 */
BuildGraphNode::BuildGraphNode(
	BuildGraph* graph,
	BuildFlag::FlagUsage usage,
	string path,
	string hash)
	: m_ref(false)
	, m_graph(graph)
	, m_toolchain("")
	, m_toolchainHash("")
	, m_hash(hash)
	, m_arch("generic")
	, m_config("generic")
	, m_name(GetBasenameOfFile(path))
	, m_script("")
	, m_path(path)
	, m_invalidInput(false)
	, m_usage(usage)
	, m_job(NULL)
	, m_finalized(false)
{
}

/**
	@brief Constructor for nodes which are object files or equivalent
 */
BuildGraphNode::BuildGraphNode(
	BuildGraph* graph,
	BuildFlag::FlagUsage usage,
	string toolchain,
	string arch,
	string name,
	string path,
	set<BuildFlag> flags)
	: m_ref(false)
	, m_graph(graph)
	, m_toolchain(toolchain)
	, m_arch(arch)
	, m_config("generic")
	, m_name(name)
	, m_script("")
	, m_path(path)
	, m_flags(flags)
	, m_invalidInput(false)
	, m_usage(usage)
	, m_job(NULL)
	, m_finalized(false)
{
	//Look up the hash of our toolchain
	m_toolchainHash = g_nodeManager->GetToolchainHash(m_arch, m_toolchain);
}

/**
	@brief Constructor for nodes which are targets or tests
 */
BuildGraphNode::BuildGraphNode(
	BuildGraph* graph,
	BuildFlag::FlagUsage usage,
	string toolchain,
	string arch,
	string config,
	string name,
	string scriptpath,
	string path,
	YAML::Node& node)
	: m_ref(false)
	, m_graph(graph)
	, m_toolchain(toolchain)
	, m_arch(arch)
	, m_config(config)
	, m_name(name)
	, m_script(scriptpath)
	, m_path(path)
	, m_usage(usage)
	, m_job(NULL)
	, m_finalized(false)
{
	//Ignore the toolchain and arches sections, they're already taken care of

	//Read the flags section
	if(node["flags"])
	{
		auto nflags = node["flags"];

		for(auto it : nflags)
		{
			//If the flag is "global" pull in the upstream flags
			string flag = it.as<std::string>();
			if(flag == "global")
				graph->GetFlags(toolchain, config, path, m_flags);
			else
				m_flags.emplace(BuildFlag(flag));
		}
	}

	//No flags specified, import base flags
	else
		graph->GetFlags(toolchain, config, path, m_flags);

	//anything else is handled in base class (source files etc)

	//Look up the hash of our toolchain
	m_toolchainHash = g_nodeManager->GetToolchainHash(m_arch, m_toolchain);
}

BuildGraphNode::~BuildGraphNode()
{
	//If we have a job, we no longer need if
	if(m_job != NULL)
	{
		m_job->Unref();
		m_job = NULL;
	}
}

/**
	@brief Finalize this node

	This involves running all dependency scans, etc. as well as computing the node's hash.
 */
void BuildGraphNode::Finalize()
{
	lock_guard<recursive_mutex> lock(m_mutex);

	if(!m_finalized)
	{
		//LogDebug("Finalizing %s\n", GetFilePath().c_str());
		string old_hash = GetHash();

		DoFinalize();

		m_graph->FinalizeCallback(this, old_hash);
	}

	m_finalized = true;

	//Update our records in the hash table

	//Update the records for us in the working copy
	m_graph->GetWorkingCopy()->UpdateFile(GetFilePath(), m_hash, false, false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GC reference marking

/**
	@brief Mark the node as referenced
 */
void BuildGraphNode::SetRef()
{
	lock_guard<recursive_mutex> lock(m_mutex);

	if(m_ref)
		return;

	m_ref = true;

	//Mark our dependencies as referenced (if they're not already)
	//Sources are a subset of dependencies, no need to process that
	auto wc = m_graph->GetWorkingCopy();
	for(auto x : m_dependencies)
	{
		auto h = wc->GetFileHash(x);

		if(m_graph->HasNodeWithHash(h))
			m_graph->GetNodeWithHash(h)->SetRef();
		else
			LogError(
				"Node %s (with hash %s) is a dependency of us\n"
				"(%s), but not in graph\n",
				x.c_str(),
				h.c_str(),
				GetFilePath().c_str());
	}
}

/// @brief Mark the node as unreferenced (do not propagate)
void BuildGraphNode::SetUnref()
{
	lock_guard<recursive_mutex> lock(m_mutex);
	m_ref = false;
}

/// @brief Checks if the node is referenced (do not propagate)
bool BuildGraphNode::IsReferenced()
{
	lock_guard<recursive_mutex> lock(m_mutex);
	return m_ref;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

string BuildGraphNode::GetIndent(int level)
{
	string ret;
	for(int i=0; i<level; i++)
		ret += "    ";
	return ret;
}

void BuildGraphNode::PrintInfo(int indentLevel)
{
	lock_guard<recursive_mutex> lock(m_mutex);
	string padded_path = GetIndent(indentLevel) + m_path;
	LogDebug("%-85s [%s]\n", padded_path.c_str(), m_hash.c_str());

	for(auto d : m_dependencies)
		m_graph->GetNodeWithPath(d)->PrintInfo(indentLevel + 1);
}

/**
	@brief Figures out what flags are applicable to a particular point in the build process
 */
void BuildGraphNode::GetFlagsForUseAt(
	BuildFlag::FlagUsage when,
	set<BuildFlag>& flags)
{
	lock_guard<recursive_mutex> lock(m_mutex);
	for(auto f : m_flags)
	{
		if(f.IsUsedAt(when))
			flags.emplace(f);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Building

/**
	@brief Actually start building this node
 */
Job* BuildGraphNode::Build(Job::Priority prio)
{
	lock_guard<recursive_mutex> lock(m_mutex);

	//If we're already building, return a new reference to the existing job
	if(m_job != NULL)
	{
		m_job->Ref();
		return m_job;
	}

	//Create a new job for us
	//Ref it again (implicit creation ref for us, second ref for the caller)
	m_job = new BuildJob(prio, m_usage, this, m_toolchainHash);
	m_job->Ref();

	//Build each of our dependencies, if needed
	set<Job*> deps;
	auto wc = m_graph->GetWorkingCopy();
	for(auto d : m_dependencies)
	{
		//Look up the graph node
		auto h = wc->GetFileHash(d);
		if(h == "")
		{
			LogError("Dependency \"%s\" is not in working copy\n", d.c_str());
			return NULL;
		}
		auto n = m_graph->GetNodeWithHash(h);

		if(n == this)
		{
			LogError("BuildGraphNode %s depends on itself!\n",
				d.c_str());
			continue;
		}

		//If the node has already been built, no action required
		auto state = n->GetOutputState();
		if(state == NodeInfo::READY)
			continue;

		//If the build failed, die now
		if(state == NodeInfo::FAILED)
			return NULL;

		//If not, build it
		deps.emplace(n->Build());
	}

	//Add dependencies to our job
	for(auto j : deps)
		m_job->AddDependency(j);

	//Submit the job to the scheduler
	g_scheduler->SubmitJob(m_job);

	//and done
	return m_job;
}
