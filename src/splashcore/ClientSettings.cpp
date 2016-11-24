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

ClientSettings* g_clientSettings = NULL;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief Default constructor (for splash/splashdev)
 */
ClientSettings::ClientSettings()
{
	//Search for the .splash directory for this project
	string dir = CanonicalizePath(".");
	while(dir != "/")
	{
		string search = dir + "/.splash";
		if(DoesDirectoryExist(search))
		{
			m_projectRoot = dir;
			break;
		}

		dir = CanonicalizePath(dir + "/..");
	}

	//If it doesn't exist, return error and quit
	if(m_projectRoot.empty())
	{
		LogError("No .splash directory found. Please run \"splash init <control server>\" from working copy root\n");
		exit(1);
	}

	//Read and parse the config
	string fname = m_projectRoot + "/.splash/config.yml";
	if(!DoesFileExist(fname))
	{
		LogError("Could not open %s. Please run \"splash init <control server>\" from working copy root\n",
			fname.c_str());
		exit(1);
	}
	string yaml = GetFileContents(fname);
	try
	{
		vector<YAML::Node> docs = YAML::LoadAll(yaml);

		//Loop over the nodes and crunch it
		for(auto doc : docs)
		{
			for(auto it : doc)
				LoadConfigEntry(it.first.as<string>(), it.second);
		}
	}
	catch(YAML::ParserException exc)
	{
		LogError("YAML parsing failed: %s\n", exc.what());
		exit(1);
	}
}

/**
	@brief Constructor used by splashbuild
 */
ClientSettings::ClientSettings(string host, int port, string uuid)
	: m_projectRoot("/dev/null")	//TODO change this?
	, m_serverHostname(host)
	, m_serverPort(port)
	, m_uuid(uuid)
{
	if(m_uuid == "")
		m_uuid = ShellCommand("uuidgen -r");
}

//TODO: Constructor that loads from command line args (for splashbuild)

ClientSettings::~ClientSettings()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Config file parsing helpers

void ClientSettings::LoadConfigEntry(string name, YAML::Node& node)
{
	//Server config
	if(name == "server")
	{
		//Should have host and port
		for(auto jt : node)
		{
			string aname = jt.first.as<string>();
			if(aname == "port")
				m_serverPort = jt.second.as<int>();
			else if(aname == "host")
				m_serverHostname = jt.second.as<string>();
			else
				LogDebug("Unrecognized config attribute %s (in server section)\n", aname.c_str());
		}
	}

	//Client config
	else if(name == "client")
	{
		//Should have UUID
		for(auto jt : node)
		{
			string aname = jt.first.as<string>();
			if(aname == "uuid")
				m_uuid = jt.second.as<string>();
			else
				LogDebug("Unrecognized config attribute %s (in client section)\n", aname.c_str());
		}
	}

	//anything else? unimplemented, ignore
	else
		LogWarning("Unrecognized config declaration %s\n", name.c_str());
}
