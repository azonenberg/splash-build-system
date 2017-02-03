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
#include "../log/log.h"

#include <sys/wait.h>

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug instrumentation

double GetTime()
{
	timespec t;
	clock_gettime(CLOCK_REALTIME,&t);
	double d = static_cast<double>(t.tv_nsec) / 1E9f;
	d += t.tv_sec;
	return d;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Protobuf I/O

/**
	@brief Convenience wrapper for sending protobuf messages and printing errors if things go bad
 */
bool SendMessage(Socket& s, const SplashMsg& msg)
{
	if(g_clientSettings == NULL)
	{
		LogError("SendMessage 2-arg form called server-side\n");
		return false;
	}
	return SendMessage(s, msg, g_clientSettings->GetServerHostname());
}

/**
	@brief Convenience wrapper for receiving protobuf messages and printing errors if things go bad
 */
bool RecvMessage(Socket& s, SplashMsg& msg)
{
	if(g_clientSettings == NULL)
	{
		LogError("RecvMessage 2-arg form called server-side\n");
		return false;
	}
	return RecvMessage(s, msg, g_clientSettings->GetServerHostname());
}

/**
	@brief Convenience wrapper for sending protobuf messages and printing errors if things go bad
 */
bool SendMessage(Socket& s, const SplashMsg& msg, string hostname)
{
	string buf;
	if(!msg.SerializeToString(&buf))
	{
		LogWarning("Connection to %s dropped (failed to serialize protobuf)\n", hostname.c_str());
		return false;
	}
	if(!s.SendPascalString(buf))
	{
		//LogWarning("Connection to %s dropped (while sending protobuf)\n", hostname.c_str());
		return false;
	}
	return true;
}

/**
	@brief Convenience wrapper for receiving protobuf messages and printing errors if things go bad
 */
bool RecvMessage(Socket& s, SplashMsg& msg, string hostname)
{
	string buf;
	if(!s.RecvPascalString(buf))
	{
		//LogWarning("Connection to %s dropped (while reading protobuf)\n", hostname.c_str());
		return false;
	}
	if(!msg.ParseFromString(buf))
	{
		LogWarning("Connection to %s dropped (failed to parse protobuf)\n", hostname.c_str());
		return false;
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// System info etc

/**
	@brief Runs a shell command and returns the output
 */
string ShellCommand(string cmd, bool trimNewline)
{
	FILE* fp = popen(cmd.c_str(), "r");
	if(fp == NULL)
	{
		LogError("popen(%s) failed\n", cmd.c_str());
		return "";
	}
	string retval;
	char line[1024];
	while(NULL != fgets(line, sizeof(line), fp))
		retval += line;
	pclose(fp);

	if(trimNewline)
		retval.erase(retval.find_last_not_of(" \n\r\t")+1);
	return retval;
}

/**
	@brief Runs a shell command and returns the exit code, putting stdout/stderr (merged) in an argument
 */
int ShellCommand(string cmd, string& stdout)
{
	int code = 42;

	//Make the pipes for stdout/err
	int stdout_pipes[2];
	if(0 != pipe(stdout_pipes))
		return -1;
	int write_end = stdout_pipes[1];
	int read_end = stdout_pipes[0];

	//Fork off the background process
	pid_t pid = fork();
	if(pid < 0)
		LogFatal("Fork failed\n");

	//We're the child process? Set up the pipes then exec the commands
	if(pid == 0)
	{
		//We're not using stdin, so close it
		close(STDIN_FILENO);

		//Close the read end of the pipe at our end
		close(read_end);

		//Copy stuff to stdout/stderr
		if(dup2(write_end, STDOUT_FILENO) < 0)
			LogFatal("dup2 #1 failed\n");
		if(dup2(write_end, STDERR_FILENO) < 0)
			LogFatal("dup2 #2 failed\n");

		//Run the process (in sh for now)
		//cmd += "; exit";
		execl("/bin/sh", "/bin/sh", "-c", cmd.c_str(), NULL);

		//If we get here, it failed
		LogError("exec failed\n");
		exit(69);
	}

	//Parent process, crunch it
	else
	{
		//Close the write end of the pipe at our end
		close(write_end);

		//Read stuff into the stdout buffer
		char tmp[1024] = {0};
		while(true)
		{
			int len = read(read_end, tmp, 1023);
			if(len <= 0)
				break;

			stdout += string(tmp, len);
		}

		//Process has terminated, clean up
		if(waitpid(pid, &code, 0) <= 0)
			LogFatal("waitpid failed\n");
	}

	//Get the exit code
	return code >> 8;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// String manipulation helpers

string MakeStringLowercase(string str)
{
	string r;
	for(auto c : str)
		r += tolower(c);
	return r;
}

/**
	@brief Split a large string into an array of lines
 */
void ParseLines(const string& str, vector<string>& lines, bool clearVector, char delim)
{
	if(clearVector)
		lines.clear();

	string s;
	size_t len = str.length();
	for(size_t i=0; i<len; i++)
	{
		char c = str[i];
		if(c == delim)
		{
			lines.push_back(s);
			s = "";
		}
		else
			s += c;
	}
	if(s != "")
		lines.push_back(s);
}

string str_replace(const string& search, const string& replace, string subject)
{
	size_t pos = 0;
	while((pos = subject.find(search, pos)) != string::npos)
	{
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}

	return subject;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Parse $PATH into a vector of strings

void ParseSearchPath(vector<string>& dirs)
{
	string path = getenv("PATH");

	string dir = "";
	for(size_t i=0; i<path.length(); i++)
	{
		if(path[i] == ':')
		{
			dirs.push_back(dir);
			dir = "";
		}
		else
			dir += path[i];
	}
	if(dir != "")
		dirs.push_back(dir);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Filename manipulation

/**
	@brief Canonicalizes a path name
 */
string CanonicalizePath(string fname)
{
	char* cpath = realpath(fname.c_str(), NULL);
	if(cpath == NULL)	//file probably does not exist
	{
		//LogDebug("Could not canonicalize path %s\n", fname.c_str());
		return "";
	}
	string str(cpath);
	free(cpath);
	return str;
}

/**
	@brief Canonicalizes a path name using pure string manipulation

	(important if the file in question may not exist, but needs to be referenced elsewhere anyway)
 */
bool CanonicalizePathThatMightNotExist(string& fname)
{
	//Tokenize at / characters
	vector<string> segments;
	ParseLines(fname, segments, true, '/');

	//Canonicalize the path segments
	//Can't use realpath() as the file doesn't actually exist in the local filesystem
	vector<string> outsegs;
	for(auto s : segments)
	{
		if(s == "..")
		{
			//Don't canonicalize off the beginning of the array
			//We don't want another MS08-067 ;)
			if(outsegs.empty())
				return false;

			else
				outsegs.erase(outsegs.end() - 1);
		}

		else
			outsegs.push_back(s);
	}

	//Combine the segments for the final output fname
	fname = "";
	for(auto s : outsegs)
		fname += s + "/";
	fname.resize(fname.length() - 1);

	return true;
}

/**
	@brief Attempts to open a directory and returns a boolean value indicating whether it exists
 */
bool DoesDirectoryExist(string fname)
{
	int hfile = open(fname.c_str(), O_RDONLY);
	if(hfile < 0)
		return false;
	bool found = true;
	struct stat buf;
	if(0 != fstat(hfile, &buf))
		found = false;
	if(!S_ISDIR(buf.st_mode))
		found = false;
	close(hfile);
	return found;
}

/**
	@brief Attempts to open a file and returns a boolean value indicating whether it exists and is readable
 */
bool DoesFileExist(string fname)
{
	int hfile = open(fname.c_str(), O_RDONLY);
	if(hfile < 0)
		return false;
	close(hfile);
	return true;
}

/**
	@brief Gets the directory component of a filename
 */
string GetDirOfFile(string fname)
{
	size_t pos = fname.rfind("/");
	if(pos == string::npos)				//if no parent directory, return empty string
		return "";
	return fname.substr(0, pos);
}

/**
	@brief Gets the basename component of a filename
 */
string GetBasenameOfFile(string fname)
{
	size_t pos = fname.rfind("/");
	return fname.substr(pos+1);
}

string GetBasenameOfFileWithoutExt(string fname)
{
	size_t pos = fname.rfind("/");
	string base = fname.substr(pos+1);
	pos = base.rfind(".");
	return base.substr(0, pos);
}

/**
	@brief Find all regular files in a given directory.
 */
void FindFiles(string dir, vector<string>& files)
{
	DIR* hdir = opendir(dir.c_str());
	if(!hdir)
		LogFatal("Directory %s could not be opened\n", dir.c_str());

	dirent ent;
	dirent* pent;
	while(0 == readdir_r(hdir, &ent, &pent))
	{
		if(pent == NULL)
			break;
		if(ent.d_name[0] == '.')
			continue;

		string fname = dir + "/" + ent.d_name;
		if(DoesDirectoryExist(fname))
			continue;

		files.push_back(fname);
	}

	//Sort the list of files to ensure determinism
	sort(files.begin(), files.end());

	closedir(hdir);
}

/**
	@brief Find all files with the specified extension in a given directory.

	The supplied extension must include the leading dot.
 */
void FindFilesByExtension(string dir, string ext, vector<string>& files)
{
	DIR* hdir = opendir(dir.c_str());
	if(!hdir)
		LogFatal("Directory %s could not be opened\n", dir.c_str());

	dirent ent;
	dirent* pent;
	while(0 == readdir_r(hdir, &ent, &pent))
	{
		if(pent == NULL)
			break;
		if(ent.d_name[0] == '.')
			continue;

		//Extension match
		string fname = dir + "/" + ent.d_name;
		if(fname.find(ext) == (fname.length() - ext.length()) )
			files.push_back(fname);
	}

	//Sort the list of files to ensure determinism
	sort(files.begin(), files.end());

	closedir(hdir);
}

/**
	@brief Find all files whose name contains the specified substring in a given directory.
 */
void FindFilesBySubstring(string dir, string sub, vector<string>& files)
{
	DIR* hdir = opendir(dir.c_str());
	if(!hdir)
		LogFatal("Directory %s could not be opened\n", dir.c_str());

	dirent ent;
	dirent* pent;
	while(0 == readdir_r(hdir, &ent, &pent))
	{
		if(pent == NULL)
			break;
		if(ent.d_name[0] == '.')
			continue;

		//Extension match
		string fname = dir + "/" + ent.d_name;
		if(fname.find(sub) != string::npos )
			files.push_back(fname);
	}

	//Sort the list of files to ensure determinism
	sort(files.begin(), files.end());

	closedir(hdir);
}

/**
	@brief Find all subdirectories in a given directory.

	@param dir			Directory to look in
	@param subdirs		Array of absolute paths to located subdirectories
 */
void FindSubdirs(string dir, vector<string>& subdirs)
{
	DIR* hdir = opendir(dir.c_str());
	if(!hdir)
		LogFatal("Directory %s could not be opened\n", dir.c_str());

	dirent ent;
	dirent* pent;
	while(0 == readdir_r(hdir, &ent, &pent))
	{
		if(pent == NULL)
			break;
		if(ent.d_name[0] == '.')	//don't find hidden dirs
			continue;

		string fname = dir + "/" + ent.d_name;
		if(!DoesDirectoryExist(fname))
			continue;

		subdirs.push_back(fname);
	}

	//Sort the list of directories to ensure determinism
	sort(subdirs.begin(), subdirs.end());

	closedir(hdir);
}

/**
	@brief Gets the relative path of a file within a directory.

	Good for printing out friendly file names etc during build.
 */
string GetRelativePathOfFile(string dir, string fname)
{
	size_t pos = fname.find(dir);
	if(pos == 0)
	{
		//Hit, remove it
		return fname.substr(dir.length() + 1);
	}
	return fname;
}

void MakeDirectoryRecursive(string path, int mode)
{
	if(!DoesDirectoryExist(path))
	{
		//Make the parent directory if needed
		string parent = GetDirOfFile(path);
		if(parent == path)
		{
			//relative path and we've hit the top, stop
			return;
		}
		if(!DoesDirectoryExist(parent))
			MakeDirectoryRecursive(parent, mode);

		//Make the directory
		if(0 != mkdir(path.c_str(), 0755))
		{
			//Do not fail if the directory was previously created
			if( (errno == EEXIST) && DoesDirectoryExist(path) )
			{}
			else
				LogFatal("Could not create directory %s (%s)\n", path.c_str(), strerror(errno));
		}
	}
}

string GetFileContents(string path)
{
	FILE* fp = fopen(path.c_str(), "rb");
	if(!fp)
	{
		LogWarning("GetFileContents: Could not open file \"%s\"\n", path.c_str());
		return "";
	}
	fseek(fp, 0, SEEK_END);
	size_t fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char* buf = new char[fsize];
	if(fsize != fread(buf, 1, fsize, fp))
	{
		LogWarning("GetFileContents: Could not read file \"%s\"\n", path.c_str());
		return "";
	}
	fclose(fp);
	string tmp(buf, fsize);		//use range constructor since file may contain null bytes
	delete[] buf;
	return tmp;
}

/**
	@brief Write a file, return false on error
 */
bool PutFileContents(string path, string data)
{
	FILE* fp = fopen(path.c_str(), "wb");
	if(!fp)
	{
		LogWarning("Couldn't create file %s\n", path.c_str());
		return false;
	}
	if(data.length() != fwrite(data.c_str(), 1, data.length(), fp))
	{
		fclose(fp);
		LogWarning("Couldn't write file %s\n", path.c_str());
		return false;
	}
	fclose(fp);
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Hashing

/**
	@brief Computes the SHA256 of a string and returns the hex hash
 */
string sha256(string str)
{
	unsigned char output_buf_raw[CryptoPP::SHA256::DIGESTSIZE];
	CryptoPP::SHA256().CalculateDigest(output_buf_raw, (unsigned char*)str.c_str(), str.length());
	string ret;
	for(int i=0; i<CryptoPP::SHA256::DIGESTSIZE; i++)
	{
		char buf[3];
		snprintf(buf, sizeof(buf), "%02x", output_buf_raw[i] & 0xFF);
		ret += buf;
	}

	return ret;
}

/**
	@brief Computes the SHA256 of a file's contents and returns the hex hash
 */
string sha256_file(string path)
{
	return sha256(GetFileContents(path));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Common network stuff

/**
	@brief Connects to the server and sends the hello transactions
 */
bool ConnectToServer(Socket& sock, ClientHello::ClientType type, string hostname_suffix)
{
	//Connect to the server
	LogVerbose("Connecting to server...\n");
	string ctl_server = g_clientSettings->GetServerHostname();
	sock.Connect(ctl_server, g_clientSettings->GetServerPort());

	if(!sock.DisableNagle())
	{
		LogWarning("Connection to %s:%d dropped (couldn't disable Nagle)\n",
			ctl_server.c_str(),
			g_clientSettings->GetServerPort());
		return false;
	}

	//Get the serverHello
	SplashMsg shi;
	if(!RecvMessage(sock, shi))
		return false;
	if(shi.Payload_case() != SplashMsg::kServerHello)
	{
		LogWarning("Connection to %s:%d dropped (expected serverHello, got %d instead)\n",
			ctl_server.c_str(),
			g_clientSettings->GetServerPort(),
			shi.Payload_case());
		return false;
	}
	auto shim = shi.serverhello();
	if(shim.magic() != SPLASH_PROTO_MAGIC)
	{
		LogWarning("Connection to %s:%d dropped (bad magic number in serverHello)\n",
			ctl_server.c_str(),
			g_clientSettings->GetServerPort());
		return false;
	}
	if(shim.version() != SPLASH_PROTO_VERSION)
	{
		LogWarning("Connection to %s:%d dropped (bad version number in serverHello)\n",
			ctl_server.c_str(),
			g_clientSettings->GetServerPort());
		return false;
	}

	//Send the clientHello
	SplashMsg chi;
	auto chim = chi.mutable_clienthello();
	chim->set_magic(SPLASH_PROTO_MAGIC);
	chim->set_version(SPLASH_PROTO_VERSION);
	chim->set_type(type);
	chim->set_hostname(ShellCommand("hostname", true) + hostname_suffix);
	chim->set_uuid(g_clientSettings->GetUUID());
	if(!SendMessage(sock, chi))
		return false;

	return true;
}

/**
	@brief Send a single-file ContentRequest
 */
bool GetRemoteFileByHash(Socket& sock, string hostname, string hash, string& content)
{
	SplashMsg creq;
	auto creqm = creq.mutable_contentrequestbyhash();
	creqm->add_hash(hash);
	if(!SendMessage(sock, creq, hostname))
		return false;

	//Wait for a response
	SplashMsg dat;
	if(!RecvMessage(sock, dat, hostname))
		return false;
	if(dat.Payload_case() != SplashMsg::kContentResponse)
	{
		LogError("Got an unexpected message (should be ContentResponse)\n");
		return false;
	}
	auto res = dat.contentresponse();
	if(res.data_size() != 1)
	{
		LogError("Got an unexpected message (should be ContentResponse of size 1)\n");
		return false;
	}

	//Process it
	auto entry = res.data(0);
	if(entry.status() != true)
	{
		LogError("File was not in cache on server (this is stupid, we were just told it was)\n");
		return false;
	}
	content = entry.data();
	return true;
}

/**
	@brief Send a bulk (multi-file) HashRequest

	@param fnames		Input file names
	@param hashes		Output map of fname -> hash.
						If a file is not present here, it was not found on the remote end.
 */
bool GetRemoteHashesByPath(Socket& sock, string hostname, set<string> fnames, map<string, string>& hashes)
{
	//Ask the server for the hashes of these files
	SplashMsg hreq;
	auto hreqm = hreq.mutable_bulkhashrequest();
	for(auto f : fnames)
		hreqm->add_fnames(f);
	if(!SendMessage(sock, hreq, hostname))
		return false;

	//Wait for response
	SplashMsg hresp;
	if(!RecvMessage(sock, hresp, hostname))
		return false;
	if(hresp.Payload_case() != SplashMsg::kBulkHashResponse)
	{
		LogWarning("Bad message (expected BulkHashResponse, got %d instead)\n", hresp.Payload_case());
		return false;
	}

	//Process the response
	auto hres = hresp.bulkhashresponse();
	for(int i=0; i<hres.files_size(); i++)
	{
		auto f = hres.files(i);

		//Complain if we couldn't find the file
		if(!f.found())
		{
			LogWarning("GetRemoteHashesByPath: Couldn't find file %s\n", f.fname().c_str());
			continue;
		}

		//Make sure the file name is valid
		string fname = f.fname();
		if(!ValidatePath(fname))
		{
			LogWarning("path %s failed to validate\n", fname.c_str());
			return false;
		}

		//Write to the output array
		hashes[fname] = f.hash();
		//LogDebug("%s has hash %s\n", fname.c_str(), f.hash().c_str());
	}

	return true;
}

/**
	@brief Send a bulk (multi-file) ContentRequest and writes them to our cache.

	The files are not written to the working directory.
 */
bool RefreshRemoteFilesByHash(Socket& sock, string hostname, map<string, string>& hashes)
{
	//Reshuffle the map so we can iterate over it with integers
	vector<string> hs;
	vector<string> fs;
	for(auto it : hashes)
	{
		//detect files that look like garbage
		auto path = it.first;
		if(!ValidatePath(path))
		{
			LogWarning("path %s failed to validate\n", path.c_str());
			return false;
		}

		//skip hashes we already have
		auto hash = it.second;
		if(g_cache->IsCached(hash))
			continue;

		fs.push_back(path);
		hs.push_back(hash);
	}

	//Ask the server for the stuff
	SplashMsg creq;
	auto creqm = creq.mutable_contentrequestbyhash();
	for(auto h : hs)
		creqm->add_hash(h);
	if(!SendMessage(sock, creq, hostname))
		return false;

	//Wait for a response
	SplashMsg dat;
	if(!RecvMessage(sock, dat, hostname))
		return false;
	if(dat.Payload_case() != SplashMsg::kContentResponse)
	{
		LogError("Got an unexpected message (should be ContentResponse)\n");
		return false;
	}
	auto res = dat.contentresponse();
	if(res.data_size() != (int)hs.size())
	{
		LogError("Got an unexpected message (should be ContentResponse of size %zu)\n", hashes.size());
		return false;
	}

	//Add results to the cache
	for(int i=0; i<res.data_size(); i++)
	{
		auto e = res.data(i);
		if(e.status() != true)
		{
			LogError("File %s was not in cache on server (this is stupid, we were just told it was)\n",
				fs[i].c_str());
			return false;
		}

		auto dat = e.data();
		g_cache->AddFile(fs[i], hs[i], sha256(dat), dat, "");
	}

	return true;
}

/**
	@brief Respond to a ContentRequest message
 */
bool ProcessContentRequest(Socket& s, string remote, SplashMsg& msg)
{
	auto creq = msg.contentrequestbyhash();

	//Create the response message
	SplashMsg reply;
	auto replym = reply.mutable_contentresponse();
	for(int i=0; i<creq.hash_size(); i++)
	{
		FileContent* entry = replym->add_data();

		//Return error if the file isn't in the cache
		string h = creq.hash(i);
		//LogDebug("Got a request for hash %s\n", h.c_str());
		if(!g_cache->IsCached(h))
			entry->set_status(false);

		//Otherwise, add the content
		else
		{
			entry->set_status(true);
			entry->set_data(g_cache->ReadCachedFile(h));
		}
	}

	//and send it
	if(!SendMessage(s, reply, remote))
		return false;

	return true;
}

/**
	@brief Makes sure a filename is a safe relative path and safe to put in a command-line arg
 */
bool ValidatePath(string fname)
{
	//Absolute paths are a biiiig no-go
	if(fname[0] == '/')
		return false;

	//Complain if there's too many ..'s
	string t = fname;
	if(!CanonicalizePathThatMightNotExist(fname))
		return false;

	//TODO: Separate function for escaping fnames passed as shell args
	//(at least spaces should be allowed eventually)
	if(fname.find(" ") != string::npos)
		return false;
	if(fname.find("\\") != string::npos)
		return false;
	if(fname.find("|") != string::npos)
		return false;
	if(fname.find("&") != string::npos)
		return false;
	if(fname.find(";") != string::npos)
		return false;
	if(fname.find("`") != string::npos)
		return false;

	return true;
}
