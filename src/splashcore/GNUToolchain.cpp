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
	// Find libraries

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

	for(auto it : m_archflags)
	{
		string arch = it.first;

		LogDebug("Finding flags for architecture %s, toolchain %s\n", arch.c_str(), exe.c_str());
		LogIndenter li;

		//Compile and link it
		map<string, string> unused;
		string sout;
		if(!Compile(exe, arch, sources, oout, flags, unused, sout, (type == GNU_CPP)))
		{
			LogError("Test compilation failed for toolchain %s\n", exe.c_str());
			continue;
		}
		sout = "";
		if(!Link(exe, arch, objects, fout, flags, unused, sout, (type == GNU_CPP), false))
		{
			LogError("Test link failed for toolchain %s:\n%s\n", exe.c_str(), sout.c_str());
			continue;
		}

		//Run readelf to see what the outputs look like
		string out;
		if(0 != ShellCommand(string("readelf -d -W ") + fout, out))
		{
			LogError("readelf failed for toolchain %s\n", exe.c_str());
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

			m_internalLibraries[arch].emplace(apath);
			m_internalLibrariesForShared[arch].emplace(apath);
		}

		//If we did not find a dynamically linked libgcc, add a static one
		if(!found_libgcc)
		{
			string libgcc_fname = ShellCommand(exe + " " + it.second + " --print-libgcc-file-name");
			m_internalLibraries[arch].emplace(CanonicalizePath(libgcc_fname));
			m_internalLibrariesForShared[arch].emplace(CanonicalizePath(libgcc_fname));
		}

		//Look for the glibc startup routines
		string crt1_fname = ShellCommand(exe + " " + it.second + " --print-file-name=crt1.o");
		m_internalLibraries[arch].emplace(CanonicalizePath(crt1_fname));
		string crti_fname = ShellCommand(exe + " " + it.second + " --print-file-name=crti.o");
		m_internalLibraries[arch].emplace(CanonicalizePath(crti_fname));
		m_internalLibrariesForShared[arch].emplace(CanonicalizePath(crti_fname));
		string crtbegin_fname = ShellCommand(exe + " " + it.second + " --print-file-name=crtbegin.o");
		m_internalLibraries[arch].emplace(CanonicalizePath(crtbegin_fname));
		string crtbeginS_fname = ShellCommand(exe + " " + it.second + " --print-file-name=crtbeginS.o");
		m_internalLibrariesForShared[arch].emplace(CanonicalizePath(crtbeginS_fname));
		string crtend_fname = ShellCommand(exe + " " + it.second + " --print-file-name=crtend.o");
		m_internalLibraries[arch].emplace(CanonicalizePath(crtend_fname));
		string crtendS_fname = ShellCommand(exe + " " + it.second + " --print-file-name=crtendS.o");
		m_internalLibrariesForShared[arch].emplace(CanonicalizePath(crtendS_fname));
		string crtn_fname = ShellCommand(exe + " " + it.second + " --print-file-name=crtn.o");
		m_internalLibraries[arch].emplace(CanonicalizePath(crtn_fname));
		m_internalLibrariesForShared[arch].emplace(CanonicalizePath(crtn_fname));
		string libcns_fname = ShellCommand(exe + " " + it.second + " --print-file-name=libc_nonshared.a");
		m_internalLibraries[arch].emplace(CanonicalizePath(libcns_fname));
		m_internalLibrariesForShared[arch].emplace(CanonicalizePath(libcns_fname));

		//Find paths to the dynamic linker.
		//This does not get put in the dependency graph since it's a runtime dependency,
		//but must be explicitly specified if we do nodefaultlib
		if(0 != ShellCommand(string("readelf -l ") + fout, out))
		{
			LogError("readelf failed for toolchain %s\n", exe.c_str());
			continue;
		}
		outs.clear();
		ParseLines(out, outs);
		out = "";
		for(auto line : outs)
		{
			if(line.find("program interpreter") != string::npos)
			{
				out = line;
				break;
			}
		}
		size_t pos = out.find(":");
		if(pos == string::npos)
			continue;
		out = out.substr(pos + 2);
		pos = out.find("]");
		if(pos == string::npos)
			continue;
		out = out.substr(0, pos);
		if(!DoesFileExist(out))
			continue;
		m_dlPaths[arch] = CanonicalizePath(out);
	}

	//Clean up
	unlink(cfn.c_str());
	unlink(oout.c_str());
	unlink(fout.c_str());
	rmdir(tmpdir.c_str());
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

		//Some compilers end paths with a dot. Trim it off
		if(line[line.length() - 1] == '.')
			paths.push_back(line.substr(1, line.length() - 2));

		else
			paths.push_back(line.substr(1));
	}

	//Debug dump
	//for(auto p : paths)
	//	LogDebug("        %s\n", p.c_str());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Meta-flag processing

/**
	@brief Convert a build flag to the actual executable flag representation
 */
string GNUToolchain::FlagToString(BuildFlag flag)
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

	//Libraries
	else if(flag.GetType() == BuildFlag::TYPE_LIBRARY)
	{
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
	//Pull out some properties we need
	string exe = chain->GetBasePath();
	string libpre = chain->GetSharedLibraryPrefix();
	string libsuf = chain->GetSharedLibrarySuffix();

	//Make sure we're good on the flags
	if(!VerifyFlags(triplet))
		return false;
	bool is_shared = flags.find(BuildFlag("output/shared")) != flags.end();

	//Look up some arch-specific stuff
	string aflags = m_archflags[triplet];
	auto apath = m_virtualSystemIncludePath[triplet];

	//Create a temporary directory to work in
	char base[] = "/tmp/splash_XXXXXX";
	string tmpdir = mkdtemp(base);

	//Create a temporary C/C++ file to build
	string cfn = tmpdir + "/test.c";
	FILE* fp = fopen(cfn.c_str(), "w");
	fprintf(fp, "int main(){return 0;}\n");
	fclose(fp);

	//Search for any library flags
	set<string> foundpaths;
	set<string> foundlibNames;
	set<BuildFlag> libflags_in;
	for(auto f : flags)
	{
		if(f.GetType() != BuildFlag::TYPE_LIBRARY)
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

	//Remove all library-related flags from the list
	for(auto f : libflags_in)
		flags.erase(f);

	//Add system libraries (but don't make -D's for them as they're implicit)
	if(is_shared)
	{
		for(auto lib : m_internalLibrariesForShared[triplet])
			foundpaths.emplace(lib);
	}
	else
	{
		for(auto lib : m_internalLibraries[triplet])
			foundpaths.emplace(lib);
	}

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

	//Clean up temp files
	unlink(cfn.c_str());
	rmdir(base);

	//Make the full scan command line
	string cmdline = exe + " " + aflags + " -M -MG ";
	for(auto f : flags)
		cmdline += FlagToString(f) + " ";
	if(cpp)
		cmdline += "-x c++ ";
	else
		cmdline += "-x c ";
	cmdline += path;
	cmdline += " 2>&1";
	//LogDebug("Dep command line: %s\n", cmdline.c_str());

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
		LogError("GNUToolchain::ScanDependencies - Make rule was not well formed (scan error?)\n");
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
	//LogDebug("Absolute paths:\n");
	//LogIndenter li;
	for(size_t i=1; i<files.size(); i++)
	{
		string f = files[i];
		//LogDebug("%s\n", f.c_str());
		LogIndenter li;

		//If the path begins with our working copy directory, trim it off and call the rest the relative path
		if(f.find(root) == 0)
		{
			//LogDebug("        local dir\n");
			f = f.substr(root.length() + 1);
		}

		//No go - loop over the system include paths
		else
		{
			//If it's located in a system include directory, that's a hit
			string longest_prefix = "";
			for(auto dir : sysdirs)
			{
				if(f.find(dir) != 0)
					continue;

				//if(dir.length() > longest_prefix.length())
				//	longest_prefix = dir;

				//Match the FIRST directory
				longest_prefix = dir;
				break;
			}

			//It's an absolute path to a standard system include directory
			if(longest_prefix != "")
			{
				//Trim off the prefix and go
				string old_f = f;
				f = apath + "/" + f.substr(longest_prefix.length() + 1);
			}

			//If it's an absolute path but NOT in a system include dir, fail.
			//Including random headers by absolute path is not portable!
			else if(f[0] == '/')
			{
				output += string("Absolute path ") + f + " could not be resolved to a system include directory\n";
				return false;
			}

			//Relative path - we don't have it locally
			//Ask the server for it (by best-guess filename)
			else if(longest_prefix == "")
			{
				string fname = str_replace(root + "/", "", GetDirOfFile(path)) + "/" + f;
				if(!CanonicalizePathThatMightNotExist(fname))
				{
					LogError("Couldn't canonicalize path %s\n", fname.c_str());
					return false;
				}

				//LogDebug("Unable to resolve include %s\n", f.c_str());
				LogDebug("Canonicalized %s to %s\n", f.c_str(), fname.c_str());
				missingFiles.emplace(fname);
				continue;
			}
		}

		//TODO: Don't even read the file if we already have it in the cache?

		//Add file to cache
		string data = GetFileContents(files[i]);
		string hash = sha256(data);
		if(!g_cache->IsCached(hash))
			g_cache->AddFile(f, hash, hash, data, "");

		//Add virtual path to output dep list
		deps.emplace(f);
		dephashes[f] = hash;
	}

	/*
	LogDebug("    Project-relative dependency paths (is_shared = %d):\n", is_shared);
	for(auto f : deps)
	{
		if(f.find("sysinclude") != string::npos)
			continue;

		LogDebug("        %s\n", f.c_str());
	}
	*/

	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual compilation

/**
	@brief Compile one or more source files to an object file
 */
bool GNUToolchain::Compile(
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
			return Link(exe, triplet, sources, fname, flags, outputs, output, cpp);
	}

	//LogDebug("Compile for arch %s\n", triplet.c_str());
	//LogIndenter li;

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
		cmdline += FlagToString(f) + " ";

	//Finish it up
	cmdline += string("-I") + apath + "/ ";		//include the virtual system path
	cmdline += "-c ";
	for(auto s : sources)
		cmdline += s + " ";
	//LogDebug("Compile command line: %s\n", cmdline.c_str());

	//Run the compile itself
	if(0 != ShellCommand(cmdline, output))
		return false;

	//Get the outputs
	vector<string> files;
	FindFiles(".", files);

	//No filtering needed, everything is toolchain output
	for(auto f : files)
	{
		f = GetBasenameOfFile(f);
		outputs[f] = sha256_file(f);
	}

	//All good if we get here
	return true;
}

bool GNUToolchain::Link(
	string exe,
	string triplet,
	set<string> sources,
	string fname,
	set<BuildFlag> flags,
	map<string, string>& outputs,
	string& output,
	bool /*cpp*/,
	bool nodefaultlib)
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

	//Pull in the dynamic linker
	if(nodefaultlib)
	{
		if(m_dlPaths.find(triplet) != m_dlPaths.end())
			aflags += " -dynamic-linker " + m_dlPaths[triplet] + " ";
	}

	//Make the full compile command line
	string cmdline = exe + " " + aflags + " -o " + fname + " ";
	for(auto f : flags)
		cmdline += FlagToString(f) + " ";

	//If we're building a shared library, set the soname
	//TODO: provide an interface for setting the library version?
	string basename = GetBasenameOfFile(fname);
	string soname = basename + ".0";
	if(is_shared && using_elf)
		cmdline += string("-Wl,-soname,") + soname + " ";

	//Do NOT link libc startup code if we're not building an executable
	//TODO: file name is glibc specific, handle other libc's?
	string startpath;
	for(auto s : sources)
	{
		if(s.find("crt1.o") != string::npos)
		{
			startpath = s;
			break;
		}
	}
	if(is_shared)
		sources.erase(startpath);

	//Ignore default libraries if we're not doing a test link
	//Explicitly pass the particular libraries passed as part of the node being built.
	if(nodefaultlib)
		cmdline += "-nodefaultlibs -nostdlib -nostartfiles ";

	//Add the object/library files to be linked
	for(auto s : sources)
		cmdline += s + " ";
	//LogDebug("Link command line: %s\n", cmdline.c_str());

	//Run the compile itself
	if(0 != ShellCommand(cmdline, output))
	{
		LogDebug("link failed\n");
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
