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

#include "splashdev.h"

using namespace std;

/**
	@brief Do something when a file changes
 */
void WatchedFileChanged(Socket& s, int type, string fname)
{
	//LogDebug("message of type %x for %s\n", type, fname.c_str());

	//TODO: handle moving/deletion of a whole directory
	if( (type & IN_MOVE_SELF) == IN_MOVE_SELF )
		LogWarning("Source directory moved - not implemented\n");
	if( (type & IN_DELETE_SELF) == IN_DELETE_SELF )
		LogWarning("Source directory deleted - not implemented\n");

	//Unused/ignored events:
	//IN_ACCESS
	//IN_ATTRIB
	//IN_CLOSE_WRITE
	//IN_CLOSE_NOWRITE
	//IN_OPEN
	
	//TODO: IN_CREATE check if we made a new directory; if so start watching it

	//TODO: IN_DELETE_SELF

	//File deleted or moved? Delete the old location from the cache
	if( ( (type & IN_DELETE) == IN_DELETE ) || ( (type & IN_MOVED_FROM) == IN_MOVED_FROM ) )
		SendDeletionNotificationForFile(s, fname);

	//File modified or moved? Send the new status
	if( ( (type & IN_MODIFY) == IN_MODIFY ) || ( (type & IN_MOVED_TO) == IN_MOVED_TO ) )
	{
		SplashMsg icn;
		BuildChangeNotificationForFile(icn.mutable_bulkfilechanged(), fname);
		if(!SendMessage(s, icn))
			return;
		SplashMsg icr;
		if(!RecvMessage(s, icr))
			return;
		if(!ProcessBulkFileAck(s, icr))
			return;
	}
}
