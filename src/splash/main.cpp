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

#include "splash.h"
#include <splashcore/SplashNet.pb.h>
#include <ext/stdio_filebuf.h>

using namespace std;

void ShowUsage();
void ShowVersion();

//Project root directory
string g_rootDir;

//Map of watch descriptors to directory names
map<int, string> g_watchMap;

int ProcessInitCommand(const vector<string>& args);

int ProcessListArchesCommand(Socket& s, const vector<string>& args);
int ProcessListClientsCommand(Socket& s, const vector<string>& args);
int ProcessListConfigsCommand(Socket& s, const vector<string>& args);
int ProcessListTargetsCommand(Socket& s, const vector<string>& args, bool pretty);
int ProcessListToolchainsCommand(Socket& s, const vector<string>& args);
int ProcessDumpGraphCommand(Socket& s, const vector<string>& args);

/**
	@brief Program entry point
 */
int main(int argc, char* argv[])
{
	Severity console_verbosity = Severity::NOTICE;

	//Parse command-line arguments
	string cmd = "";
	vector<string> args;
	bool nobanner = false;
	for(int i=1; i<argc; i++)
	{
		string s(argv[i]);

		//Let the logger eat its args first
		if(ParseLoggerArguments(i, argc, argv, console_verbosity))
			continue;

		else if(s == "--help")
		{
			ShowUsage();
			return 0;
		}

		else if(s == "--version")
		{
			ShowVersion();
			return 0;
		}

		else if(s == "--nobanner")
			nobanner = true;

		//Whatever it is, it's a command (TODO handle args)
		else if(cmd.empty())
			cmd = s;

		else
			args.push_back(s);
	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new STDLogSink(console_verbosity));

	//Print header
	if( (console_verbosity >= Severity::NOTICE) && !nobanner )
	{
		ShowVersion();
		printf("\n");
	}

	//If the command is "init" we have to process it BEFORE loading the config or connecting to the server
	//since the config doesn't yet exist!
	if(cmd == "init")
		return ProcessInitCommand(args);

	//Load the configuration so we know where the server is, etc
	g_clientSettings = new ClientSettings;

	//Connect to the server
	Socket sock(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if(!ConnectToServer(sock, ClientHello::CLIENT_UI))
		return 1;

	//Send the devInfo
	SplashMsg devi;
	auto devim = devi.mutable_devinfo();
	devim->set_arch(ShellCommand("dpkg-architecture -l | grep DEB_HOST_GNU_TYPE | cut -d '=' -f 2", true));
	if(!SendMessage(sock, devi))
		return 1;

	//Process other commands once the link is up and running
	if(cmd == "dump-graph")
		return ProcessDumpGraphCommand(sock, args);
	else if(cmd == "list-arches")
		return ProcessListArchesCommand(sock, args);
	else if(cmd == "list-clients")
		return ProcessListClientsCommand(sock, args);
	else if(cmd == "list-configs")
		return ProcessListConfigsCommand(sock, args);
	else if(cmd == "list-targets")
		return ProcessListTargetsCommand(sock, args, true);
	else if(cmd == "list-targets-simple")
		return ProcessListTargetsCommand(sock, args, false);
	else if(cmd == "list-toolchains")
		return ProcessListToolchainsCommand(sock, args);

	//Clean up and finish
	delete g_clientSettings;
	return 0;
}

/**
	@brief Handles "splash init"
 */
int ProcessInitCommand(const vector<string>& args)
{
	//Sanity check
	if(args.size() < 1)
	{
		LogError("Missing arguments. Usage:  \"splash init <control server> [control server port]\"\n");
		return 1;
	}

	//Assume the current directory is the working copy root
	string dir = CanonicalizePath(".");
	LogNotice("Initializing working copy at %s\n", dir.c_str());

	//Make the config directory
	string splashdir = dir  + "/.splash";
	MakeDirectoryRecursive(splashdir, 0700);

	//Parse the args
	string server = args[0];	//TODO: force to be valid hostname
	string port = "49000";
	if(args.size() > 1)
		port = args[1];			//TODO: force to be integer

	//Write the final config.yml
	string cfgpath = splashdir + "/config.yml";
	string config = "server:\n";
	config +=		"    host: " + server + "\n";
	config +=		"    port: " + port + "\n";
	config +=       "\n";
	config +=		"client:\n";
	config +=		"    uuid: \"" + ShellCommand("uuidgen -r") + "\"\n";
	PutFileContents(cfgpath, config);

	//We're good
	return 0;
}

/**
	@brief Handles "splash list-toolchains"
 */
int ProcessListToolchainsCommand(Socket& s, const vector<string>& args)
{
	//Sanity check
	if(args.size() != 0)
	{
		LogError("Extra arguments. Usage:  \"splash list-toolchains\"\n");
		return 1;
	}

	//Format the command
	SplashMsg cmd;
	auto cmdm = cmd.mutable_inforequest();
	cmdm->set_type(InfoRequest::TOOLCHAIN_LIST);
	if(!SendMessage(s, cmd))
		return 1;

	//Get the response back
	SplashMsg msg;
	if(!RecvMessage(s, msg))
		return 1;
	if(msg.Payload_case() != SplashMsg::kToolchainList)
	{
		LogError("Got wrong message type back\n");
		return 1;
	}

	auto lt = msg.toolchainlist();
	LogNotice("%-20s %-15s %-15s %-25s %-25s %-30s\n", "Version", "Hash", "Languages", "Name", "Architectures", "Nodes");
	for(int i=0; i<lt.infos_size(); i++)
	{
		auto info = lt.infos(i);

		//Figure out how many rows we need
		int nnames = info.names_size();
		int nclients = info.uuids_size();
		int nlangs = info.langs_size();
		int narches = info.arches_size();
		int nmax = max(nnames, nclients);
		nmax = max(nmax, nlangs);
		nmax = max(nmax, narches);

		//Print each row
		LogNotice(
			"---------------------------------------------------------------------"
			"---------------------------------------------------\n");
		for(int j=0; j<nmax; j++)
		{
			//Truncate hash for display
			string hash;
			if(j == 0)
				hash = string("...") + info.hash().substr(55);

			string version;
			if(j == 0)
				version = info.version();

			string name;
			if(j < nnames)
				name = info.names(j);

			string lang;
			if(j < nlangs)
				lang = info.langs(j);

			string arch;
			if(j < narches)
				arch = info.arches(j);

			string uuid;
			if(j < nclients)
				uuid = info.uuids(j);

			LogNotice(
				"%-20s %-15s %-15s %-25s %-25s %-30s\n",
				version.c_str(),
				hash.c_str(),
				lang.c_str(),
				name.c_str(),
				arch.c_str(),
				uuid.c_str());
		}
	}

	//all good
	return 0;
}

/**
	@brief Handles "splash list-targets"
 */
int ProcessListTargetsCommand(Socket& s, const vector<string>& args, bool pretty)
{
	//Sanity check
	if(args.size() != 0)
	{
		LogError("Extra arguments. Usage:  \"splash list-targets\"\n");
		return 1;
	}

	//Format the command
	SplashMsg cmd;
	auto cmdm = cmd.mutable_inforequest();
	cmdm->set_type(InfoRequest::TARGET_LIST);
	if(!SendMessage(s, cmd))
		return 1;

	//Get the response back
	SplashMsg msg;
	if(!RecvMessage(s, msg))
		return 1;
	if(msg.Payload_case() != SplashMsg::kTargetList)
	{
		LogError("Got wrong message type back\n");
		return 1;
	}

	//Pretty-print
	if(pretty)
	{
		auto lt = msg.targetlist();
		LogNotice("%-30s %-15s %-30s\n", "Target", "Toolchain", "Script");
		for(int i=0; i<lt.info_size(); i++)
		{
			auto info = lt.info(i);
			LogNotice(
				"%-30s %-15s %-20s\n",
				info.name().c_str(),
				info.toolchain().c_str(),
				info.script().c_str());
		}
	}

	//Simple print
	else
	{
		auto lt = msg.targetlist();
		for(int i=0; i<lt.info_size(); i++)
			LogNotice("%s\n", lt.info(i).name().c_str());
	}

	//all good
	return 0;
}

/**
	@brief Handles "splash list-configs"
 */
int ProcessListConfigsCommand(Socket& s, const vector<string>& args)
{
	//Sanity check
	if(args.size() != 0)
	{
		LogError("Extra arguments. Usage:  \"splash list-configs\"\n");
		return 1;
	}

	//TODO: do "list configs for target"

	//Format the command
	SplashMsg cmd;
	auto cmdm = cmd.mutable_inforequest();
	cmdm->set_type(InfoRequest::CONFIG_LIST);
	if(!SendMessage(s, cmd))
		return 1;

	//Get the response back
	SplashMsg msg;
	if(!RecvMessage(s, msg))
		return 1;
	if(msg.Payload_case() != SplashMsg::kConfigList)
	{
		LogError("Got wrong message type back\n");
		return 1;
	}

	//and process it
	auto lt = msg.configlist();
	for(int i=0; i<lt.configs_size(); i++)
		LogNotice("%s\n", lt.configs(i).c_str());

	//all good
	return 0;
}


/**
	@brief Handles "splash list-configs"
 */
int ProcessListClientsCommand(Socket& s, const vector<string>& args)
{
	//Sanity check
	if(args.size() != 0)
	{
		LogError("Extra arguments. Usage:  \"splash list-clients\"\n");
		return 1;
	}

	//Format the command
	SplashMsg cmd;
	auto cmdm = cmd.mutable_inforequest();
	cmdm->set_type(InfoRequest::CLIENT_LIST);
	if(!SendMessage(s, cmd))
		return 1;

	//Get the response back
	SplashMsg msg;
	if(!RecvMessage(s, msg))
		return 1;
	if(msg.Payload_case() != SplashMsg::kClientList)
	{
		LogError("Got wrong message type back\n");
		return 1;
	}

	auto lt = msg.clientlist();
	LogNotice("%-15s %-20s %-30s\n", "Type", "Hostname", "UUID");
	for(int i=0; i<lt.infos_size(); i++)
	{
		auto info = lt.infos(i);

		string stype;
		switch(info.type())
		{
			case ClientHello::CLIENT_DEVELOPER:
				stype = "splashdev";
				break;

			case ClientHello::CLIENT_BUILD:
				stype = "splashbuild";
				break;

			case ClientHello::CLIENT_UI:
				stype = "splash";
				break;

			default:
				stype = "<error>";
				break;
		}

		LogNotice(
			"%-15s %-20s %-30s\n",
			stype.c_str(),
			info.hostname().c_str(),
			info.uuid().c_str());
	}

	//all good
	return 0;
}

/**
	@brief Handles "splash list-arches"
 */
int ProcessListArchesCommand(Socket& s, const vector<string>& args)
{
	//Sanity check
	if(args.size() > 1)
	{
		LogError("Extra arguments. Usage:  \"splash list-arches [target]\"\n");
		return 1;
	}

	//Format the command
	SplashMsg cmd;
	auto cmdm = cmd.mutable_inforequest();
	cmdm->set_type(InfoRequest::ARCH_LIST);
	if(args.size() == 1)
		cmdm->set_query(args[0]);
	if(!SendMessage(s, cmd))
		return 1;

	//Get the response back
	SplashMsg msg;
	if(!RecvMessage(s, msg))
		return 1;
	if(msg.Payload_case() != SplashMsg::kArchList)
	{
		LogError("Got wrong message type back\n");
		return 1;
	}

	//and process it
	auto lt = msg.archlist();
	for(int i=0; i<lt.arches_size(); i++)
		LogNotice("%s\n", lt.arches(i).c_str());

	//all good
	return 0;
}

/**
	@brief Handles "splash dump-graph"
 */
int ProcessDumpGraphCommand(Socket& s, const vector<string>& args)
{
	//Sanity check
	if(args.size() != 0)
	{
		LogError("Extra arguments. Usage:  \"splash list-clients\"\n");
		return 1;
	}

	//Format the command
	SplashMsg cmd;
	auto cmdm = cmd.mutable_inforequest();
	cmdm->set_type(InfoRequest::NODE_LIST);
	if(!SendMessage(s, cmd))
		return 1;

	//Get the response back
	SplashMsg msg;
	if(!RecvMessage(s, msg))
		return 1;
	if(msg.Payload_case() != SplashMsg::kNodeList)
	{
		LogError("Got wrong message type back\n");
		return 1;
	}

	//and process it
	auto lt = msg.nodelist();
	/*
	for(int i=0; i<lt.arches_size(); i++)
		LogNotice("%s\n", lt.arches(i).c_str());
	*/

	//all good
	return 0;
}

void ShowVersion()
{
	printf(
		"SPLASH workstation client by Andrew D. Zonenberg.\n"
		"\n"
		"License: 3-clause BSD\n"
		"This is free software: you are free to change and redistribute it.\n"
		"There is NO WARRANTY, to the extent permitted by law.\n");
}

void ShowUsage()
{
	printf(
		"Usage: splash [options] <command> [args]\n"
		"\n"
		"options may be zero or more of the following:\n"
		"    --debug                        Sets log level to debug\n"
		"    -l, --logfile <fname>          Directs logging to the file <fname>\n"
		"    -L, --logfile-lines <fname>    Directs logging to the file <fname> and\n"
		"                                   turns on line buffering\n"
		"    --nobanner                     Do not print header/license at startup\n"
		"    -q, --quiet                    Decreases log verbosity by one step\n"
		"    --verbose                      Sets log level to verbose\n"
		"\n"
		"command may be one of the following:\n"
		"    dump-graph                     Print the entire dependency graph to stdout\n"
		"                                   in graphviz format.\n"
		"    init                           Initialize a new working copy\n"
		"    list-arches                    List all architectures we have at least\n"
		"                                   one target for.\n"
		"    list-clients                   List all clients connected to the server.\n"
		"    list-configs                   List all configurations we have at least\n"
		"                                   one target for.\n"
		"    list-targets                   List all targets in the working copy,\n"
		"                                   pretty printed with details\n"
		"    list-targets-simple            List all targets in the working copy,\n"
		"                                   with one target name per line\n"
		"    list-toolchains                List all toolchains the server knows about.\n"
		"\n"
		"splash init <control server> [port]\n"
		"    Initializes the .splash directory within this working copy to store\n"
		"    client-side configuration. This must be the first Splash command\n"
		"    executed in the working copy after cloning.\n"
		"\n"
		"    control server: Hostname of the splashctl server. Required.\n"
		"    port: Port number of the splashctl server. Defaults to 49000.\n"
	);
	exit(0);
}