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

#ifndef BuildGraphNode_h
#define BuildGraphNode_h

/**
	@brief A single node in the build graph
 */
class BuildGraphNode
{
public:
	BuildGraphNode();
	virtual ~BuildGraphNode();
	
	/// @brief Get the hash of the node
	std::string GetHash()
	{ return m_hash; }
	
	/// @brief Get the script of the node
	std::string GetScript()
	{ return m_script; }

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Garbage collection during graph rebuilds

	/// @brief Mark the node as referenced
	void SetRef()
	{ m_ref = true; }
	
	/// @brief Mark the node as unreferenced
	void SetUnref()
	{ m_ref = false; }
	
	/// @brief Checks if the node is referenced
	bool IsReferenced()
	{ return m_ref; }
	
protected:

	/// @brief Indicates if the node is referenced
	bool m_ref;

	/// @brief The hash of this node (must be set in constructor; immutable)
	std::string m_hash;
	
	/// @brief Path to the build script this node was declared in
	std::string m_script;
};

#endif
