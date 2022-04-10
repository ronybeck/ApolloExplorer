/*
 * MakeDir.c
 *
 *  Created on: May 27, 2021
 *      Author: rony
 */

#include "MakeDir.h"

#define DBGOUT 0

#ifdef __GNUC__
#if DBGOUT
#include <stdio.h>
#endif
#include <proto/dos.h>
#endif

#include "VNetUtil.h"
#include "protocol.h"
#include "protocolTypes.h"



#ifdef __VBCC__

#endif

char makeDir( char *dirPath )
{
	//What if the path already exists?
	BPTR dirLock = Lock( dirPath, ACCESS_READ );
	if( dirLock )
	{
		//The dir already exists.  This is good
		UnLock( dirLock );
		return 1;
	}

	//Attempt to create the directory
	dirLock = CreateDir( dirPath );
	if( dirLock )
	{
		UnLock( dirLock );
		return 1;
	}

	//looks like we couldn't create this for some reason.
	return 0;
}
