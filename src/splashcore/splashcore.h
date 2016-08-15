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

#include <string>
#include <vector>
#include <list>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Other library includes

#include <crypto++/sha.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Global functions

double GetTime();

void FatalError(const char* format, ...);

std::string CanonicalizePath(std::string fname);
bool DoesDirectoryExist(std::string fname);
bool DoesFileExist(std::string fname);
std::string GetDirOfFile(std::string fname);
std::string GetBasenameOfFile(std::string fname);
std::string GetBasenameOfFileWithoutExt(std::string fname);
void FindFilesByExtension(std::string dir, std::string ext, std::vector<std::string>& files);
void FindSubdirs(std::string dir, std::vector<std::string>& subdirs);
std::string GetRelativePathOfFile(std::string dir, std::string fname);

void MakeDirectoryRecursive(std::string path, int mode);

std::string sha256(std::string str);
std::string sha256_file(std::string path);

#endif
