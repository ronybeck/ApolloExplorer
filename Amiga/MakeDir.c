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

#include "AEUtil.h"
#include "protocol.h"
#include "protocolTypes.h"



#ifdef __VBCC__

#endif

char makeDir( char *dirPath )
{
	//What if the path already exists?
	char returnCode = AT_NOK;
	BPTR dirLock = Lock( dirPath, ACCESS_READ );
	if( dirLock )
	{
		//OK this path already exists.   But is it a directory?
		struct FileInfoBlock fileInfoBlock;
		dbglog( "makeDir() Path %s already exists.\n", dirPath );
		if( Examine( dirLock, &fileInfoBlock ) )
		{
			dbglog( "makeDir() Examining %s\n", dirPath );
			if( fileInfoBlock.fib_DirEntryType < 0 )
			{
				dbglog( "makeDir() Path %s is already a file.  We can't make a directory here.\n", dirPath );
				returnCode = AT_DEST_EXISTS_AS_FILE;
			} else
			{
				returnCode = AT_OK;
			}
		}
		UnLock( dirLock );
		return returnCode;
	}

	//Attempt to create the directory
	dirLock = CreateDir( dirPath );
	if( dirLock )
	{
		//We succeeded in creating the directory.
		UnLock( dirLock );
		returnCode = AT_OK;
	} else
	{
		//We failed to create the directory for some reason.  But that reason is not clear.
		returnCode = AT_NOK;
	}

	//looks like we couldn't create this for some reason.
	return returnCode;
}
