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
// Debug output

double GetTime()
{
	timespec t;
	clock_gettime(CLOCK_REALTIME,&t);
	double d = static_cast<double>(t.tv_nsec) / 1E9f;
	d += t.tv_sec;
	return d;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug logging (for now)

void FatalError(const char* format, ...)
{
	fprintf(stderr, "\033[0mERROR: ");
	
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	
	//trap to debugger
	fflush(stdout);
	fflush(stderr);
	asm("int3");
	
	exit(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Filename manipulation

/**
	@brief Canonicalizes a path name
 */
string CanonicalizePath(string fname)
{
	char* cpath = realpath(fname.c_str(), NULL);
	if(cpath == NULL)
	{
		FatalError("Could not canonicalize path %s\n", fname.c_str());
		return fname;
	}
	string str(cpath);
	free(cpath);
	return str;
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
	@brief Find all files with the specified extension in a given directory.
	
	The supplied extension must include the leading dot.
 */
void FindFilesByExtension(string dir, string ext, vector<string>& files)
{
	DIR* hdir = opendir(dir.c_str());
	if(!hdir)
		FatalError("Directory %s could not be opened\n", dir.c_str());
	
	dirent ent;
	dirent* pent;
	while(0 == readdir_r(hdir, &ent, &pent))
	{
		if(pent == NULL)
			break;
		if(ent.d_name[0] == '.')
			continue;
			
		//Extension match
		string fname = CanonicalizePath(dir + "/" + ent.d_name);
		if(fname.find(ext) == (fname.length() - ext.length()) )
			files.push_back(fname);
	}
	
	//Sort the list of files to ensure determinism
	sort(files.begin(), files.end());
	
	closedir(hdir);
}

/**
	@brief Find all subdirectories in a given directory.
 */
void FindSubdirs(string dir, vector<string>& subdirs)
{
	DIR* hdir = opendir(dir.c_str());
	if(!hdir)
		FatalError("Directory %s could not be opened\n", dir.c_str());
	
	dirent ent;
	dirent* pent;
	while(0 == readdir_r(hdir, &ent, &pent))
	{
		if(pent == NULL)
			break;
		if(ent.d_name[0] == '.')	//don't find hidden dirs
			continue;
		
		string fname = CanonicalizePath(dir + "/" + ent.d_name);
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
				FatalError("Could not create directory %s (%s)\n", path.c_str(), strerror(errno));
		}
	}
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
	FILE* fp = fopen(path.c_str(), "rb");
	if(!fp)
		FatalError("sha256_file: Could not open file \"%s\"\n", path.c_str());
	fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char* buf = new char[fsize + 1];
	buf[fsize] = 0;
	fread(buf, 1, fsize, fp);
	fclose(fp);
	string tmp(buf);
	delete[] buf;
	return sha256(tmp);
}
