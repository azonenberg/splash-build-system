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

#include "splashbuild-launcher.h"

using namespace std;

void ShowUsage();
void ShowVersion();

/**
	@brief Program entry point
 */
int main(int argc, char* argv[])
{
	Severity console_verbosity = Severity::NOTICE;

	string ctl_server;

	//Parse command-line arguments
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

		//Last arg without switch is control server.
		//TODO: mandatory arguments to introduce these?
		else
			ctl_server = argv[i];

	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	//Print header
	if(console_verbosity >= Severity::NOTICE)
	{
		ShowVersion();
		printf("\n");
	}

	if(ctl_server == "")
	{
		LogError("Usage: splashbuild-launcher [options] ctl_server\n");
		return 1;
	}

	//Create a UUID
	string uuid = ShellCommand("uuidgen -r");

	//Figure out how many virtual CPUs we have
	string scount = ShellCommand("cat /proc/cpuinfo  | grep vendor_id | wc -l");
	int cpucount = atoi(scount.c_str());
	LogNotice("Found %d virtual CPU cores, starting workers...\n", cpucount);

	//Launch the apps
	vector<pid_t> daemons;
	for(int i=0; i<cpucount; i++)
	{
		//Fork off the background process
		pid_t pid = fork();
		if(pid < 0)
			LogFatal("Fork failed\n");

		//We're the child process? Set up the files then exec the commands
		if(pid == 0)
		{
			//We're not using stdin/out/err, so pipe them to /dev/null
			int hnull = open("/dev/null", O_RDWR);
			if(hnull < 0)
				LogFatal("couldnt open /dev/null\n");
			if(dup2(hnull, STDIN_FILENO) < 0)
				LogFatal("dup2 #1 failed\n");
			if(dup2(hnull, STDOUT_FILENO) < 0)
				LogFatal("dup2 #2 failed\n");
			if(dup2(hnull, STDERR_FILENO) < 0)
				LogFatal("dup2 #3 failed\n");

			//Open the logfile
			char logpath[32];
			snprintf(logpath, sizeof(logpath), "/tmp/splashbuild-%d.log", i);

			//Find the directory our exe is in (splashbuild should be in the same spot)
			string launcher_path = CanonicalizePath("/proc/self/exe");
			string splashbuild_path = GetDirOfFile(launcher_path) + "/splashbuild";

			//Format the node number as ascii for the argument
			char snodenum[32];
			snprintf(snodenum, sizeof(snodenum), "%d", i);

			//Run the process
			execl(
				splashbuild_path.c_str(),
				"splashbuild",
				"--debug",
				ctl_server.c_str(),
				"--nodenum",
				snodenum,
				"--uuid",
				uuid.c_str(),
				"--logfile-lines",
				logpath,
				NULL);

			//If we get here, it failed
			LogError("exec failed\n");
			exit(69);
		}

		//Parent process, save results
		else
			daemons.push_back(pid);
	}

	//Wait for user to press a key
	LogNotice("Build daemons running, press any key to exit...\n");
	fflush(stdout);
	struct termios oldt, newt;
	tcgetattr ( STDIN_FILENO, &oldt );
	newt = oldt;
	newt.c_lflag &= ~( ICANON | ECHO );
	tcsetattr ( STDIN_FILENO, TCSANOW, &newt );
	getchar();
	tcsetattr ( STDIN_FILENO, TCSANOW, &oldt );

	//Kill the build servers
	for(auto pid : daemons)
	{
		kill(pid, SIGQUIT);

		int code;
		if(waitpid(pid, &code, 0) <= 0)
			LogError("waitpid failed\n");
	}

	return 0;
}

void ShowVersion()
{
	printf(
		"SPLASH build server launcher by Andrew D. Zonenberg.\n"
		"\n"
		"License: 3-clause BSD\n"
		"This is free software: you are free to change and redistribute it.\n"
		"There is NO WARRANTY, to the extent permitted by law.\n");
}

void ShowUsage()
{
	printf("Usage: splashbuild-launcher ctlserver\n");
	exit(0);
}
