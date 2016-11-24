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

#ifndef ClientSettings_h
#define ClientSettings_h

/**
	@brief Settings for this client (stored in $WORKING_COPY/.splash/config.yml)
 */
class ClientSettings
{
public:

	ClientSettings();
	ClientSettings(std::string host, int port, std::string uuid = "");

	virtual ~ClientSettings();

	/// @brief Return project root directory
	std::string GetProjectRoot()
	{ return m_projectRoot; }

	/// @brief Return server hostname
	std::string GetServerHostname()
	{ return m_serverHostname; }

	/// @brief Return server port
	int GetServerPort()
	{ return m_serverPort; }

	/// @brief Return client UUID
	std::string GetUUID()
	{ return m_uuid; }

protected:

	void LoadConfigEntry(std::string name, YAML::Node& node);

	/// @brief Root directory of the current working copy
	std::string m_projectRoot;

	/// @brief Hostname of the server
	std::string m_serverHostname;

	/// @brief Port number of the server
	int m_serverPort;

	/// @brief UUID of this client
	std::string m_uuid;
};

/**
	@brief The global settings object.

	Must be explicitly created by the end application (defaults to NULL).
 */
extern ClientSettings* g_clientSettings;

#endif
