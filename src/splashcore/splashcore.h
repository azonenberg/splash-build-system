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

#ifndef splashcore_h
#define splashcore_h

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Platform includes

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// libc includes

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <typeinfo>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// libstdc++ includes

#include <list>
#include <mutex>
#include <string>
#include <sstream>
#include <unordered_set>
#include <vector>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Other library includes

#include <crypto++/sha.h>
#include <yaml-cpp/yaml.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Project includes

#include "../log/log.h"
#include "../xptools/Socket.h"

#include "Cache.h"

#include "Toolchain.h"

#include "GNUToolchain.h"

#include "BuildFlag.h"
#include "BuildConfiguration.h"
#include "BuildSettings.h"

#include "ToolchainSettings.h"

#include "CToolchain.h"

#include "CPPToolchain.h"
#include "FPGAToolchain.h"
#include "GNUCToolchain.h"
#include "GNUCPPToolchain.h"
#include "GNULinkerToolchain.h"
#include "LinkerToolchain.h"
#include "RemoteToolchain.h"
#include "XilinxISEToolchain.h"
#include "XilinxVivadoToolchain.h"

#include "BuildGraph.h"
#include "BuildGraphNode.h"
#include "CPPSourceNode.h"
#include "CPPObjectNode.h"
#include "CPPExecutableNode.h"

#include "WorkingCopy.h"

#include "NodeManager.h"

#include "Job.h"
#include "BuildJob.h"
#include "DependencyScanJob.h"
#include "Scheduler.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Protocol stuff

#define SPLASH_PROTO_MAGIC		0x444c4942
#define SPLASH_PROTO_VERSION	1

#include <splashcore/SplashNet.pb.h>

bool SendMessage(Socket& s, const SplashMsg& msg, std::string hostname);
bool RecvMessage(Socket& s, SplashMsg& msg, std::string hostname);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Global functions

double GetTime();

void ParseLines(std::string str, std::vector<std::string>& lines, bool clearVector = false);
std::string MakeStringLowercase(std::string str);

std::string ShellCommand(std::string cmd, bool trimNewline = true);

std::string str_replace(const std::string& search, const std::string& replace, std::string subject);

void ParseSearchPath(std::vector<std::string>& dirs);

std::string CanonicalizePath(std::string fname);
bool DoesDirectoryExist(std::string fname);
bool DoesFileExist(std::string fname);
std::string GetDirOfFile(std::string fname);
std::string GetBasenameOfFile(std::string fname);
std::string GetBasenameOfFileWithoutExt(std::string fname);

void FindFiles(std::string dir, std::vector<std::string>& files);
void FindFilesBySubstring(std::string dir, std::string sub, std::vector<std::string>& files);
void FindFilesByExtension(std::string dir, std::string ext, std::vector<std::string>& files);
void FindSubdirs(std::string dir, std::vector<std::string>& subdirs);

std::string GetRelativePathOfFile(std::string dir, std::string fname);

void MakeDirectoryRecursive(std::string path, int mode);

std::string sha256(std::string str);
std::string sha256_file(std::string path);

std::string GetFileContents(std::string path);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Global data

extern std::map<std::string, Toolchain*> g_toolchains;

#endif
