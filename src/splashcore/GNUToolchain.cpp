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

#include "splashcore.h"

using namespace std;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief Figure out what flags we have to pass to the compiler and linker in order to build for a specific architecture.

	An empty string indicates this is the default.

	ALL supported triplets must be listed here; the absence of an entry is an error.
 */
GNUToolchain::GNUToolchain(string arch, string exe, GNUType type)
{
	/////////////////////////////////////////////////////////////////////////////
	// X86

	if(arch == "x86_64-linux-gnu")
	{
		m_archflags["x86_64-linux-gnu"] 	= "-m64";
		m_archflags["x86_64-linux-gnux32"]	= "-mx32";
		m_archflags["i386-linux-gnu"]		= "-m32";
	}

	else if(arch == "x86_64-w64-mingw32")
	{
		m_archflags[arch] = "";
	}

	else if(arch == "i386-linux-gnu")
	{
		m_archflags[arch] = "";
	}

	/////////////////////////////////////////////////////////////////////////////
	// MIPS

	else if(arch == "mips-elf")
	{
		m_archflags["mips-elf"] 	= "-EB";
		m_archflags["mipsel-elf"] 	= "-EL";
	}

	/////////////////////////////////////////////////////////////////////////////
	// ARM

	else if(arch == "arm-linux-gnueabihf")
	{
		m_archflags[arch] = "";
	}

	else if(arch == "arm-linux-gnueabi")
	{
		m_archflags[arch] = "";
	}

	else if(arch == "arm-none-eabi")
	{
		m_archflags[arch] = "";
	}

	/////////////////////////////////////////////////////////////////////////////
	// INVALID

	else
	{
		LogWarning("Don't know what flags to use for target %s\n", arch.c_str());
	}


	/////////////////////////////////////////////////////////////////////////////
	// Find and hash libc (TODO: support non-GNU libc's?)

	//If it's a linker, do nothing
	if(type == GNU_LD)
		return;

	//Create a temporary directory to work in
	char base[] = "/tmp/splash_XXXXXX";
	string tmpdir = mkdtemp(base);
	string oout = tmpdir + "/test.o";
	string fout = tmpdir + "/test";

	//Create a temporary C/C++ file to build
	string cfn = tmpdir + "/test.c";
	FILE* fp = fopen(cfn.c_str(), "w");
	fprintf(fp, "int main(){return 0;}\n");
	fclose(fp);

	//Figure out file names
	set<string> sources;
	sources.emplace(cfn);
	set<string> objects;
	objects.emplace(oout);

	set<BuildFlag> flags;

	set<string> badarches;
	for(auto it : m_archflags)
	{
		string arch = it.first;

		LogDebug("Finding flags for architecture %s, toolchain %s\n", arch.c_str(), exe.c_str());
		LogIndenter li;

		//Compile and link it
		map<string, string> unused;
		string sout;
		if(!Compile(NULL, exe, arch, sources, oout, flags, unused, sout, (type == GNU_CPP)))
		{
			LogError("Test compilation failed for toolchain %s\n", exe.c_str());
			badarches.emplace(arch);
			continue;
		}
		sout = "";
		if(!Link(NULL, exe, arch, objects, fout, flags, unused, sout, (type == GNU_CPP)))
		{
			LogError("Test link failed for toolchain %s:\n%s", exe.c_str(), sout.c_str());
			badarches.emplace(arch);
			continue;
		}

		//Run readelf to see what the outputs look like
		string out;
		if(0 != ShellCommand(string("readelf -d -W ") + fout, out))
		{
			LogError("readelf failed for toolchain %s\n", exe.c_str());
			badarches.emplace(arch);
			continue;
		}
		vector<string> outs;
		ParseLines(out, outs);

		//Parse the output
		string slp = "[";
		bool found_libgcc = false;
		for(auto line : outs)
		{
			auto pos = line.find(slp);
			if(pos == string::npos)
				continue;

			string n = line.substr(pos + slp.length());
			auto e = n.find("]");
			if(e == string::npos)
				continue;

			string libname = n.substr(0, e);

			//Look up the absolute path of the library
			string apath = CanonicalizePath(ShellCommand(exe + " " + it.second + " --print-file-name=" + libname));

			if(apath.find("libgcc") != string::npos)
				found_libgcc = true;

			m_internalLibs += sha256_file(apath);
		}

		//If we did not find a dynamically linked libgcc, add a static one
		if(!found_libgcc)
		{
			string libgcc_fname = ShellCommand(exe + " " + it.second + " --print-libgcc-file-name");
			m_internalLibs += sha256_file(libgcc_fname);
		}

		//Look for the glibc startup routines
		string crt1_fname = ShellCommand(exe + " " + it.second + " --print-file-name=crt1.o");
		if(DoesFileExist(crt1_fname))
			m_internalLibs += sha256_file(crt1_fname);
		string crti_fname = ShellCommand(exe + " " + it.second + " --print-file-name=crti.o");
		if(DoesFileExist(crti_fname))
			m_internalLibs += sha256_file(crti_fname);
		string crtbegin_fname = ShellCommand(exe + " " + it.second + " --print-file-name=crtbegin.o");
		if(DoesFileExist(crtbegin_fname))
			m_internalLibs += sha256_file(crtbegin_fname);
		string crtbeginS_fname = ShellCommand(exe + " " + it.second + " --print-file-name=crtbeginS.o");
		if(DoesFileExist(crtbeginS_fname))
			m_internalLibs += sha256_file(crtbeginS_fname);
		string crtend_fname = ShellCommand(exe + " " + it.second + " --print-file-name=crtend.o");
		if(DoesFileExist(crtend_fname))
			m_internalLibs += sha256_file(crtend_fname);
		string crtendS_fname = ShellCommand(exe + " " + it.second + " --print-file-name=crtendS.o");
		if(DoesFileExist(crtendS_fname))
			m_internalLibs += sha256_file(crtendS_fname);
		string crtn_fname = ShellCommand(exe + " " + it.second + " --print-file-name=crtn.o");
		if(DoesFileExist(crtn_fname))
			m_internalLibs += sha256_file(crtn_fname);
		string libcns_fname = ShellCommand(exe + " " + it.second + " --print-file-name=libc_nonshared.a");
		if(DoesFileExist(libcns_fname))
			m_internalLibs += sha256_file(libcns_fname);
	}

	//Remove architectures we couldn't compile for during discovery
	for(auto a : badarches)
		m_archflags.erase(a);

	//Clean up
	unlink(cfn.c_str());
	unlink(oout.c_str());
	unlink(fout.c_str());
	rmdir(tmpdir.c_str());
}

string GNUToolchain::ParseStringVersion(string basepath)
{
	//Get the full compiler version
	//Example strings:
	//* gcc (GCC) 4.8.5 20150623 (Red Hat 4.8.5-11)
	//* gcc (Debian 4.9.2-10) 4.9.2
	string sver = ShellCommand(basepath + " --version | head -n 1 | cut -d \")\" -f 2");

	//Trim off leading spaces
	while(!sver.empty() && isspace(sver[0]))
		sver = sver.substr(1);

	//Trim off any parenthetical blob at the end (if present
	size_t pos = sver.find("(");
	if(pos != string::npos)
		sver = sver.substr(0, pos-2);

	return sver;
}

bool GNUToolchain::HasValidArches()
{
	return !m_archflags.empty();
}

void GNUToolchain::FindDefaultIncludePaths(vector<string>& paths, string exe, bool cpp, string arch)
{
	//LogDebug("    Finding default include paths for %s (c++=%d)\n", arch.c_str(), cpp);

	//We must have valid flags for this arch
	if(!VerifyFlags(arch))
		return;
	string aflags = m_archflags[arch];

	//Ask the compiler what the paths are
	vector<string> lines;
	string cmd = exe + " " + aflags + " -c --verbose ";
	if(cpp)
		cmd += "-x c++ ";
	else
		cmd += "-x c ";
	cmd += "- < /dev/null 2>&1";
	ParseLines(ShellCommand(cmd), lines);

	//Look for the beginning of the search path list
	size_t i = 0;
	for(; i<lines.size(); i++)
	{
		auto line = lines[i];
		if(line.find("starts here") != string::npos)
			break;
	}

	//Get the actual paths
	for(; i<lines.size(); i++)
	{
		auto line = lines[i];

		if(line == "End of search list.")
			break;
		if(line[0] == '#')
			continue;

		string searchdir;

		//Some compilers end paths with a dot. Trim it off
		if(line[line.length() - 1] == '.')
			searchdir = line.substr(1, line.length() - 2);

		else
			searchdir = line.substr(1);

		//Canonicalize the path. This is important b/c Red Hat etc sometimes have symlinks in these paths
		//and we need to make sure we're looking in the canonical directory
		paths.push_back(CanonicalizePath(searchdir));
	}

	//Add lib directory because distros are derpy and sometimes put headers here
	string dir = string("/usr/lib/") + arch;
	if(DoesDirectoryExist(dir))
		paths.push_back(CanonicalizePath(dir));

	//Debug dump
	LogTrace("Standard include paths:\n");
	int ipath = 0;
	for(auto p : paths)
	{
		char tmp[128];
		snprintf(tmp, sizeof(tmp), "search%d", ipath);
		ipath ++;
		m_searchdirRemap[p] = tmp;

		LogTrace("    %s (%s)\n", p.c_str(), tmp);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Meta-flag processing

/**
	@brief Convert a build flag to the actual executable flag representation
 */
string GNUToolchain::FlagToString(BuildFlag flag, string triplet)
{
	string s = flag;

	//Optimization flags
	if(s == "optimize/none")
		return "-O0";
	else if(s == "optimize/speed")
		return "-O2";

	//Debug info flags
	else if(s == "debug/gdb")
		return "-ggdb";

	//Language dialect
	else if(s == "dialect/c++11")
		return "--std=c++11";

	//Output file formats
	else if(s == "output/shared")
		return "-shared";
	else if(s == "output/reloc")
		return "-fPIC";

	//Dynamic analysis
	else if(s == "analysis/sanitize/address")
		return "-fsanitize=address";

	//Libraries
	else if(flag.GetType() == BuildFlag::TYPE_LIBRARY)
	{
		//Special processing for incpkg
		//TODO: can this be more cross platform? It's a giant pain in the butt
		if(flag.GetFlag() == "incpkg")
			return "-I/usr/include/" + flag.GetArgs() + " -I/usr/lib/" + triplet + "/" + flag.GetArgs() + "/include";

		//The library is specified by absolute path on the link command line as a source file,
		//so there's no extra flags required here
		return "";
	}

	//Macro definitions
	else if(flag.GetType() == BuildFlag::TYPE_DEFINE)
	{
		return string("-D") + flag.GetFlag() + "=" + flag.GetArgs();
	}

	//Warning levels
	else if(s == "warning/max")
		return "-Wall -Wextra -Wcast-align -Winit-self -Wmissing-declarations -Wswitch -Wwrite-strings";

	else
	{
		LogWarning("GNUToolchain does not implement flag %s yet\n", s.c_str());
		return "";
	}
}

/**
	@brief
 */
bool GNUToolchain::VerifyFlags(string triplet)
{
	//We must have valid flags for this arch
	if(m_archflags.find(triplet) == m_archflags.end())
	{
		LogError("Don't know how to target %s\n", triplet.c_str());
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual operations

/**
	@brief Scans a source file for dependencies (include files, etc)
 */
bool GNUToolchain::ScanDependencies(
	Toolchain* chain,
	string triplet,
	string path,
	string root,
	set<BuildFlag> flags,
	const vector<string>& sysdirs,
	set<string>& deps,
	map<string, string>& dephashes,
	string& output,
	set<string>& missingFiles,
	set<BuildFlag>& libFlags,
	bool cpp)
{
	//Bump profiling count
	m_timesScanned[path] ++;

	//Pull out some properties we need
	string exe = chain->GetBasePath();

	//Make sure we're good on the flags
	if(!VerifyFlags(triplet))
	{
		output = "ERROR: Flag verification failed\n";
		return false;
	}

	//Look up some arch-specific stuff
	string aflags = m_archflags[triplet];
	auto apath = m_virtualSystemIncludePath[triplet];

	//Look up directories specified by flags - gcc is derpy and sometimes doesn't go full absolute for these
	//(this must happen before we search for library flags)
	//TODO: make incpkg not a library flag since it works at scan time?
	set<string> flagdirs;
	for(auto f : flags)
	{
		if( (f.GetType() == BuildFlag::TYPE_LIBRARY) && (f.GetFlag() == "incpkg") )
		{
			flagdirs.emplace(string("/usr/include/") + f.GetArgs());
			flagdirs.emplace(string("/usr/lib/") + triplet + "/" + f.GetArgs() + "/include");
		}
		//else
		//	LogTrace("flag %s is not a flagdir\n", static_cast<string>(f).c_str());
	}
	//for(auto f : flagdirs)
	//	LogTrace("flag dir: %s\n", f.c_str());

	//Search for any library flags
	set<string> foundpaths;
	set<string> foundlibNames;
	set<BuildFlag> libflags_in;
	if(!FindLibraries(chain, triplet, flags, foundpaths, foundlibNames, libflags_in, output))
	{
		output += "ERROR: Library searching failed\n";
		return false;
	}

	//Remove all library-related flags from the list
	for(auto f : libflags_in)
		flags.erase(f);

	//Add the found libraries to our cache if needed, and create dependencry records for them
	for(auto fpath : foundpaths)
	{
		//Calculate virtual path
		string libpath = string("__syslib__/") + str_replace(" ", "_", chain->GetVersionString()) + "_" + triplet;
		string f = libpath + "/" + GetBasenameOfFile(fpath);

		//Add file to cache
		string data = GetFileContents(fpath);
		string hash = sha256(data);
		if(!g_cache->IsCached(hash))
			g_cache->AddFile(f, hash, hash, data, "");

		//Add virtual path to output dep list
		deps.emplace(f);
		dephashes[f] = hash;
	}

	//If we specified library/required, and didn't find the lib, declare an error now
	//rather than wasting time trying to compile and/or link
	for(auto f : libflags_in)
	{
		if(f.GetFlag() != "required")
			continue;

		string lbase = f.GetArgs();
		if(foundlibNames.find(lbase) == foundlibNames.end())
		{
			char tmp[1024];
			snprintf(tmp, sizeof(tmp),
				"Required library \"%s\" was not found\n",
				lbase.c_str());
			output += tmp;
			return false;
		}
	}

	//Add HAVE_FOO defines for each library
	for(auto base : foundlibNames)
	{
		string mname = base;
		for(size_t i=0; i<mname.length(); i++)
		{
			if(!isalnum(mname[i]))
				mname[i] = 'x';
			else
				mname[i] = toupper(mname[i]);
		}

		string flagname = string("define/HAVE_") + mname;
		flags.emplace(BuildFlag(flagname));
		libFlags.emplace(BuildFlag(flagname));
	}

	//Make the full scan command line
	string cmdline = exe + " " + aflags + " -M -MG ";
	for(auto f : flags)
		cmdline += FlagToString(f, triplet) + " ";
	if(cpp)
		cmdline += "-x c++ ";
	else
		cmdline += "-x c ";
	cmdline += path;
	cmdline += " 2>&1";
	LogTrace("Dep command line: %s\n", cmdline.c_str());

	//Run it
	string makerule;
	if(0 != ShellCommand(cmdline, makerule))
	{
		output = makerule;
		return false;
	}
	//Parse it
	size_t offset = makerule.find(':');
	if(offset == string::npos)
	{
		output += "ERROR: GNUToolchain::ScanDependencies - Make rule was not well formed (scan error?)\n";
		return false;
	}
	vector<string> files;
	string tmp;
	for(size_t i=offset + 1; i<makerule.length(); i++)
	{
		bool last_was_slash = (makerule[i-1] == '\\');

		//Skip leading spaces
		char c = makerule[i];
		if( tmp.empty() && isspace(c) )
			continue;

		//If not a space, it's part of a file name
		else if(!isspace(c))
			tmp += c;

		//Space after a slash is part of a file name
		else if( (c == ' ') && last_was_slash)
			tmp += c;

		//Any other space is a delimiter
		else
		{
			if(tmp != "\\")
				files.push_back(tmp);
			tmp = "";
		}
	}
	if(!tmp.empty() && (tmp != "\\") )
		files.push_back(tmp);

	//First entry is always the source file itself, so we can skip that
	//Loop over the other entries and convert them to system/project relative paths
	LogTrace("Absolute paths:\n");
	LogIndenter li;
	for(size_t i=1; i<files.size(); i++)
	{
		//Canonicalize the path since Red Hat looooves symlinks!
		//We need to make sure we resolve files to system include paths even if the actual system path
		//is written differently, as long as the point to the same spot
		string f = files[i];
		if(DoesFileExist(f))
			f = CanonicalizePath(f);

		LogTrace("%s\n", f.c_str());
		LogIndenter li;

		//If the path begins with our working copy directory, trim it off and call the rest the relative path
		if(f.find(root) == 0)
		{
			//LogTrace("local dir\n");
			f = f.substr(root.length() + 1);
		}

		//No go - loop over the system include paths
		else
		{
			//If it's located in a system include directory, that's a hit
			string longest_prefix = "";
			for(auto dir : sysdirs)
			{
				//Need to search for the trailing / so we don't match prefixed names
				//example: foo/include-fixed should NOT hit on foo/include
				if(f.find(dir + "/") != 0)
					continue;

				//Don't match longer prefixes if they have "backward" in them.
				//(some kind of weird glibc quirk)
				if(dir.find("backward") == string::npos)
				{
					if(dir.length() > longest_prefix.length())
						longest_prefix = dir;
				}

				//Match the first prefix we find regardless
				if(longest_prefix == "")
					longest_prefix = dir;
			}

			//Even if we're in a system include directory, if we're in a -I directory that is longer,
			//match that one instead.
			if(longest_prefix != "")
			{
				for(auto flagdir : flagdirs)
				{
					if(f.find(flagdir) != 0)
						continue;

					//We found it!
					LogTrace("Resolved path to sysinclude subdir %s\n", flagdir.c_str());
					if(flagdir.length() > longest_prefix.length())
						longest_prefix = flagdir;
				}
			}

			//It's an absolute path to a standard system include directory. Trim off the prefix and go.
			if(longest_prefix != "")
			{
				//Is it a compiler include dir? Remap to the virtual search directory
				//so we preserve ordering for include_next
				if(m_searchdirRemap.find(longest_prefix) != m_searchdirRemap.end())
					f = apath + "/" + m_searchdirRemap[longest_prefix] + "/" + f.substr(longest_prefix.length() + 1);

				//No, probably a -I or something. No remapping.
				else
					f = apath + "/" + f.substr(longest_prefix.length() + 1);
			}

			//If it's an absolute path but NOT in a system include dir, fail.
			//Including random headers by absolute path is not portable!
			else if(f[0] == '/')
			{
				output += string("Absolute path ") + f + " could not be resolved to a system include directory\n";
				return false;
			}

			//Relative path - we probably don't have it locally
			//Ask the server for it (by best-guess filename)
			else if(longest_prefix == "")
			{
				LogTrace("Trying to resolve relative path %s\n", f.c_str());
				LogIndenter li;

				//Nope, maybe it's a project relative path?
				string fname = str_replace(root + "/", "", GetDirOfFile(path)) + "/" + f;
				if(!CanonicalizePathThatMightNotExist(fname))
				{
					output += string("Couldn't canonicalize path ") + fname + "\n";
					return false;
				}
				LogTrace("Trying %s\n", fname.c_str());

				//It seems like sometimes #include <> (vs "") doesn't resolve to an absolute path sometimes :(
				//Check if we have the file in the working directory
				if(DoesFileExist(fname))
				{
					LogTrace("Resolved relative to current directory\n");
					files[i] = fname;
					f = fname;
				}

				else
				{
					LogTrace("Unable to resolve include %s\n", f.c_str());
					missingFiles.emplace(fname);
					continue;
				}
			}
		}

		//Add file to cache
		string data = GetFileContents(files[i]);
		string hash = sha256(data);
		if(!g_cache->IsCached(hash))
			g_cache->AddFile(f, hash, hash, data, "");

		if(!CanonicalizePathThatMightNotExist(f))
		{
			output += string("Couldn't canonicalize path ") + f + "\n";
			return false;
		}

		//Add virtual path to output dep list
		deps.emplace(f);
		dephashes[f] = hash;
	}

	LogTrace("    Project-relative dependency paths:\n");
	for(auto f : deps)
	{
		//if(f.find("sysinclude") != string::npos)
		//	continue;

		LogTrace("        %s\n", f.c_str());
	}

	//Debug profiling: Dump total # of times each thing was scanned
	/*
	LogTrace("Total scan count:\n");
	{
		LogIndenter li;
		for(auto it : m_timesScanned)
			LogTrace("%-80s: %d\n", it.first.c_str(), it.second);
	}
	*/

	return true;
}

/**
	@brief Search for any libraries mentioned in our flags
 */
bool GNUToolchain::FindLibraries(
	Toolchain* chain,
	string triplet,
	set<BuildFlag> flags,
	set<string>& foundpaths,
	set<string>& foundlibNames,
	set<BuildFlag>& libflags_in,
	string& output)
{
	//Pull out some properties we need
	string exe = chain->GetBasePath();
	string libpre = chain->GetPrefix("shlib");
	string libsuf = chain->GetSuffix("shlib");
	string aflags = m_archflags[triplet];

	//Create a temporary directory to work in
	char base[] = "/tmp/splash_XXXXXX";
	string tmpdir = mkdtemp(base);

	//Create a temporary C/C++ file to build
	string cfn = tmpdir + "/test.c";
	FILE* fp = fopen(cfn.c_str(), "w");
	fprintf(fp, "int main(){return 0;}\n");
	fclose(fp);

	for(auto f : flags)
	{
		if(f.GetType() != BuildFlag::TYPE_LIBRARY)
			continue;

		//Don't filter off incpkg since this is used for dependency scanning
		if(f.GetFlag() == "incpkg")
			continue;

		string libbase = f.GetArgs();
		libflags_in.emplace(f);

		//If we have a cached result from this library, don't scan for it again
		pair<string, string> index(triplet, libbase);
		if(m_libpaths.find(index) != m_libpaths.end())
		{
			string path = m_libpaths[index];

			//If the cache says "not found", nothing to do
			if(path == "")
				continue;

			//We found it, record the results
			foundlibNames.emplace(libbase);
			foundpaths.emplace(path);
		}

		//Not in cache, have to search
		else
		{
			//Get library file name
			//TODO: search static libraries too?
			string libname = libpre + libbase + libsuf;
			if(!ValidatePath(libname))
			{
				char tmp[1024];
				snprintf(tmp, sizeof(tmp),
					"Library path %s was malformed\n",
					libname.c_str());
				output += tmp;
				return false;
			}

			//Use the compiler to find it
			string fpath;
			if(0 != ShellCommand(exe + " " + aflags + " --print-file-name=" + libname, fpath))
			{
				char tmp[1024];
				snprintf(tmp, sizeof(tmp),
					"Failed to search for library %s\n",
					libname.c_str());
				output += tmp;
				return false;
			}
			while(isspace(fpath[fpath.length()-1]))
				fpath.resize(fpath.length() - 1);

			//See if the file exists. GCC will print a path even if it doesn't!
			if(!DoesFileExist(fpath))
			{
				m_libpaths[index] = "";
				continue;
			}

			//We're good, find the actual path
			fpath = CanonicalizePath(fpath);

			//At this point we know the library exists, but we don't yet know if it's compatible
			//with our selected architecture.
			//Do a test compile to find out
			string ignored;
			if(0 != ShellCommand(exe + " " + aflags + " -o /dev/null " + cfn + " " + fpath, ignored))
			{
				m_libpaths[index] = "";
				continue;
			}

			//Record our results
			m_libpaths[index] = fpath;
			foundlibNames.emplace(libbase);
			foundpaths.emplace(fpath);
		}
	}

	//Clean up temp files
	unlink(cfn.c_str());
	rmdir(base);

	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual compilation

/**
	@brief Compile one or more source files to an object file
 */
bool GNUToolchain::Compile(
	Toolchain* chain,
	string exe,
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& output,
	bool cpp)
{
	//If any source has a .o extension, we're linking and not compiling
	for(auto s : sources)
	{
		if(s.find(".o") != string::npos)
			return Link(chain, exe, triplet, sources, fname, flags, outputs, output, cpp);
	}

	LogTrace("Compile for arch %s\n", triplet.c_str());
	LogIndenter li;

	if(!VerifyFlags(triplet))
		return false;

	//Look up some arch-specific stuff
	string aflags = m_archflags[triplet];
	auto apath = m_virtualSystemIncludePath[triplet];

	//Figure out compile flags
	string cmdline = exe + " " + aflags + " -o " + fname + " ";
	cmdline += "-nostdinc ";					//make sure we only include files the server provided
	cmdline += "-nostdinc++ ";
	if(cpp)
		cmdline += "-x c++ ";
	else
		cmdline += "-x c ";
	for(auto f : flags)							//special flags
		cmdline += FlagToString(f, triplet) + " ";

	//Finish it up.
	//Include a bunch of virtual system include paths.
	//TODO: can we transmit the number present on the golden node somehow? For now, just try ten and hope for the best
	cmdline += string("-I") + apath + "/ ";
	for(int i=0; i<10; i++)
	{
		char tmp[128];
		snprintf(tmp, sizeof(tmp), "search%d", i);
		cmdline += string("-I") + apath + "/" + tmp + "/ ";
	}

	//TODO: include other dirs like $PROJECT etc
	cmdline += "-c ";
	for(auto s : sources)
		cmdline += s + " ";
	LogTrace("Compile command line: %s\n", cmdline.c_str());

	//Run the compile itself
	if(0 != ShellCommand(cmdline, output))
	{
		LogTrace("Build failed!\n%s\n", output.c_str());
		return false;
	}

	//Get the outputs
	vector<string> files;
	FindFiles(".", files);

	//No filtering needed, everything is toolchain output
	for(auto f : files)
	{
		f = GetBasenameOfFile(f);
		LogTrace("Build output: %s\n", f.c_str());
		outputs[f] = sha256_file(f);
	}

	//All good if we get here
	return true;
}

bool GNUToolchain::Link(
	Toolchain* chain,
	string exe,
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& output,
	bool /*cpp*/)
{
	//LogDebug("Link for arch %s\n", triplet.c_str());
	LogIndenter li;

	if(!VerifyFlags(triplet))
		return false;

	//Look up some arch-specific stuff
	//Link using gcc/g++ instead of ld
	string aflags = m_archflags[triplet];

	bool is_shared = flags.find(BuildFlag("output/shared")) != flags.end();
	bool using_elf = (triplet.find("linux") != string::npos) || (triplet.find("elf") != string::npos);

	//Make the base command line
	string cmdline = exe + " " + aflags + " -o " + fname + " ";
	for(auto f : flags)
		cmdline += FlagToString(f, triplet) + " ";

	//If we're building a shared library, set the soname
	//TODO: provide an interface for setting the library version?
	string basename = GetBasenameOfFile(fname);
	string soname = basename + ".0";
	if(is_shared && using_elf)
		cmdline += string("-Wl,-soname,") + soname + " ";

	//Add the object/library files to be linked.
	//Do some reordering to ensure objects come before libs.
	set<string> objects;
	set<string> libs;
	string sosuf;
	string asuf;
	if(chain)
	{
		chain->GetSuffix("shlib");
		chain->GetSuffix("stlib");
	}
	for(auto s : sources)
	{
		//If toolchain is null we're being called by the ctor
		//and obviously not linking any libraries.
		//In this case, assume the input is a source file
		if(chain && s.find(sosuf) != string::npos)
			libs.emplace(s);
		else if(chain && s.find(asuf) != string::npos)
			libs.emplace(s);
		else
			objects.emplace(s);
	}

	for(auto s : objects)
		cmdline += s + " ";
	for(auto s : libs)
		cmdline += s + " ";

	//LogDebug("Link command line: %s\n", cmdline.c_str());

	//Run the link itself
	if(0 != ShellCommand(cmdline, output))
	{
		//LogDebug("link failed\n");
		//LogError("%s\n", output.c_str());
		return false;
	}

	//Get the outputs
	vector<string> files;
	FindFiles(".", files);

	//No filtering needed, everything is toolchain output
	for(auto f : files)
	{
		f = GetBasenameOfFile(f);
		outputs[f] = sha256_file(f);
	}

	//If we built a shared library, make soname copy
	if(is_shared && using_elf)
	{
		outputs[soname] = outputs[basename];
		string stuff = GetFileContents(fname);
		PutFileContents(soname, stuff);
	}

	//All good if we get here
	return true;
}
