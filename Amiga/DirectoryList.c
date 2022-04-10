/*
 * DirectoryList.c
 *
 *  Created on: May 21, 2021
 *      Author: rony
 */

#include "DirectoryList.h"


#define DBGOUT 1

#ifdef __GNUC__
#if DBGOUT
#include <stdio.h>
#endif
#include <string.h>
#include <proto/dos.h>
#include <proto/exec.h>
#endif

#ifdef __VBCC__

#endif


#include "VNetUtil.h"

static unsigned getDirectoryNumberOfEntries( char *path, unsigned int *filenameBufferLen  )
{
	unsigned int directoryListSize = 0;
	*filenameBufferLen = 0;
	dbglog( "[getDirectoryNumberOfEntries] Getting directory list for path %s.\n", path );

	BPTR dirLock = 0;
	struct FileInfoBlock fileInfoBlock;

	//Let's see if we don't already have this directory
	dirLock = Lock( path, ACCESS_READ );
	if( !dirLock )
	{
		//This directory doesn't exist.  Send back an empty answer
		dbglog( "[getDirectoryNumberOfEntries] The directory %s doesn't exist?\n", path );
		return 0;
	}

	//Is this a directory?
	if( !Examine( dirLock, &fileInfoBlock ) || fileInfoBlock.fib_DirEntryType < 1 )
	{
		UnLock( dirLock );
		dbglog( "[getDirectoryNumberOfEntries]The path '%s' is not a directory.", path );

		return 0;
	}

	//Create an ExAllControl Object
	struct ExAllControl *control = AllocDosObject(DOS_EXALLCONTROL,NULL);
	if( !control )
	{
		UnLock( dirLock );
		dbglog( "[getDirectoryNumberOfEntries] Unable to create a ExAllControl object.\n" );
		return 0;
	}

	//Iterate through the entries until we are the end
	ULONG bufferLen =  sizeof( struct ExAllData ) * 5;
	struct ExAllData *buffer = AllocVec( bufferLen, MEMF_FAST|MEMF_CLEAR );
	struct ExAllData *currentEntry = NULL;
	BOOL more;
	control->eac_LastKey = 0;
	do
	{
		more = ExAll( dirLock, buffer, bufferLen, ED_COMMENT, control );
		if ((!more) && (IoErr() != ERROR_NO_MORE_ENTRIES))
		{
			break;
		}
		if( control->eac_Entries == 0 )	continue;

		//Go through the current batch of entries
		currentEntry = buffer;
		while( currentEntry )
		{
			switch( currentEntry->ed_Type )
			{
				case ST_FILE:
				case ST_USERDIR:
				case ST_ROOT:
				case ST_SOFTLINK:
					directoryListSize++;
					break;
				default:
					break;
			}
			*filenameBufferLen += (unsigned int)strlen( (char*)currentEntry->ed_Name );
			dbglog( "Found file '%s'.\n", (char*)currentEntry->ed_Name );
			currentEntry = currentEntry->ed_Next;
		}
	}while( more );
	FreeDosObject( DOS_EXALLCONTROL, control );
	FreeVec( buffer );
	UnLock( dirLock );

	//Return our result
	dbglog( "[getDirectoryNumberOfEntries] There are %d entries in '%s' and a buffer of %d bytes is needed for the names.\n", directoryListSize, path, *filenameBufferLen );
	return directoryListSize;
}


ProtocolMessageDirectoryList_t *getDirectoryList( char *path )
{
	BPTR dirLock = 0;
	struct FileInfoBlock fileInfoBlock;
	ProtocolMessageDirectoryList_t *dirListMsg= NULL;
	unsigned int directoryListSize = 0;
	unsigned int filenameBufferSize = 0;
	char *currentPositionInMessage = 0;
	char *endOfMessageMemory = 0;
	unsigned int sizeOfCurrentMessage = 0;

	dbglog( "[getDirectoryList] Getting directory list for path %s.\n", path );

	//get the number of entries
	directoryListSize = getDirectoryNumberOfEntries( path, &filenameBufferSize );
	dbglog( "[getDirectoryList] Directory '%s' contains %d entries and requires %d bytes to store the names.\n", path, directoryListSize, filenameBufferSize );

	//The first entry we want to have contains the path we requested.  So increase our counts accordingly
	directoryListSize++;
	filenameBufferSize+=strlen( path ) + 1;

	//Allocate memory for the message
	size_t messageSize = sizeof( ProtocolMessageDirectoryList_t ) + (sizeof( ProtocolMessage_DirEntry_t ) * directoryListSize ) + filenameBufferSize;
	dirListMsg = ( ProtocolMessageDirectoryList_t* )AllocVec( messageSize, MEMF_FAST|MEMF_CLEAR );
	endOfMessageMemory = ((char*)dirListMsg) + messageSize;
	//dbglog( "[getDirectoryList] dirListMsg: 0x%08x\n", dirListMsg );
	//dbglog( "[getDirectoryList] dirListMsg size: %d bytes\n", messageSize );


	//Let's set what we already know into the message
	dirListMsg->header.token = MAGIC_TOKEN;
	dirListMsg->header.type = PMT_DIR_LIST;
	dirListMsg->header.length = messageSize;
	dirListMsg->entryCount = directoryListSize;
	currentPositionInMessage = (char*)&dirListMsg->entries[ 0 ];

	//If the directory is empty, we can exit here
	if( directoryListSize == 0 )
	{
		dbglog( "[getDirectoryList] Directory '%s' is empty.\n", path );
		return dirListMsg;
	}

	//Set the starting memory pointer for our entries
	ProtocolMessage_DirEntry_t *entry = (ProtocolMessage_DirEntry_t*)currentPositionInMessage;

	//Let's create the first message for our current directory
	entry->entrySize = sizeof( ProtocolMessage_DirEntry_t ) + strlen( path );
	//entry->filenameLength = strlen( path ) + 1;
	strncpy( entry->filename, path, strlen( path ) + 1 );
	entry->size = 0;
	entry->type = 0;
	dbglog( "[getDirectoryList] entry->filename: %s\n", entry->filename );
	dbglog( "[getDirectoryList] entry->entrySize: %d\n", entry->entrySize );

	//Now set the pointer for the next message
	entry = (ProtocolMessage_DirEntry_t*)((char*)entry + entry->entrySize );
	dbglog( "[getDirectoryList] entry address: 0x%08x\n", (unsigned int)entry );

	//Let's see if we don't already have this directory
	dirLock = Lock( path, ACCESS_READ );
	if( !dirLock )
	{
		//This directory doesn't exist.  Send back an empty answer
		dbglog( "[getDirectoryList] The directory %s doesn't exist?\n", path );
		return dirListMsg;
	}

	//Is this a directory?
	if( !Examine( dirLock, &fileInfoBlock ) || fileInfoBlock.fib_DirEntryType < 1 )
	{
		UnLock( dirLock );
		dbglog( "[getDirectoryList]The path '%s' is not a directory.", path );

		return dirListMsg;
	}

	//Create an ExAllControl Object
	struct ExAllControl *control = AllocDosObject(DOS_EXALLCONTROL,NULL);
	//char *matchString = "#?";
	if( !control )
	{
		UnLock( dirLock );
		dbglog( "[getDirectoryList] Unable to create a ExAllControl object.\n" );
		return 0;
	}



	//Iterate through the entries until we are the end
	ULONG bufferLen =  sizeof( struct ExAllData ) * 5;
	dbglog( "[getDirectoryList] Opening directory %s to scan\n", path );
	struct ExAllData *buffer = AllocVec( bufferLen, MEMF_FAST|MEMF_CLEAR );
	struct ExAllData *currentEntry = NULL;
	//int debugLimit=20;
	control->eac_LastKey = 0;
	BOOL more;
	do
	{
		more = ExAll( dirLock, buffer, bufferLen, ED_COMMENT, control );
		if ((!more) && (IoErr() != ERROR_NO_MORE_ENTRIES))
		{
			dbglog( "[getDirectoryList] No more entries.\n" );
			break;
		}
		if( control->eac_Entries == 0 )	continue;

		//Go through the current batch of entries
		currentEntry = buffer;
		while( currentEntry )
		{
			dbglog( "[getDirectoryList] Found " );
			switch( currentEntry->ed_Type )
			{
				case ST_FILE:
					dbglog( "file " );
					directoryListSize++;
					entry->type = DET_FILE;
					break;
				case ST_USERDIR:
					dbglog( "dir " );
					directoryListSize++;
					entry->type = DET_USERDIR;
					break;
				case ST_ROOT:
					dbglog( "root " );
					directoryListSize++;
					entry->type = DET_ROOT;
					break;
				case ST_SOFTLINK:
					dbglog( "softlink " );
					directoryListSize++;
					entry->type = DET_SOFTLINK;
					break;
				default:
					dbglog( "unknown type " );
					break;
			}

			dbglog( "[getDirectoryList] '%s' which is %ld bytes in size.\n", currentEntry->ed_Name, currentEntry->ed_Size );

			//Now add this as an entry in our list message
			//entry->filenameLength = strlen( currentEntry->ed_Name );
			entry->entrySize = sizeOfCurrentMessage = (unsigned int)sizeof( ProtocolMessage_DirEntry_t ) + strlen( currentEntry->ed_Name );	//Note: because of the char[1] member of the ProtocolMessage_DirEntry_t we don't need an extra byte for the'\0'
			if( ((char*)entry) + sizeOfCurrentMessage > endOfMessageMemory )
			{
				dbglog( "[getDirectoryList] Trying to write past end of message by %d ", (((char*)entry) + sizeOfCurrentMessage) - endOfMessageMemory );
				break;
			}
			entry->size = currentEntry->ed_Size;
			strncpy( entry->filename, currentEntry->ed_Name, strlen( (const char *)currentEntry->ed_Name ) + 1 );
			entry->filename[ strlen( currentEntry->ed_Name ) ] = 0;	//Make sure we get the terminating NULL there
			dbglog( "[getDirectoryList] entry->filename: %s\n", entry->filename );
			dbglog( "[getDirectoryList] entry->entrySize: %d\n", entry->entrySize );
			//dbglog( "[getDirectoryList] dirListMsg->header.token: 0x%08x\n", dirListMsg->header.token );


			//Now let's move onto the next one
			entry = (ProtocolMessage_DirEntry_t*)((char*)entry + entry->entrySize );
			dbglog( "[getDirectoryList] Next entry address: 0x%08x\n", (unsigned int)entry );
			currentEntry = currentEntry->ed_Next;
		}
	}while( more );
	FreeDosObject( DOS_EXALLCONTROL, control );
	FreeVec( buffer );

	dbglog( "[getDirectoryList] There are %d entries in '%s'.\n", directoryListSize, path );
	dbglog( "[getDirectoryList] Unlocking directory.\n" );
	UnLock( dirLock );
	dbglog( "[getDirectoryList] Directory unlocked.\n" );
	dbglog( "[getDirectoryList] Returning directory listing message of size %d bytes.\n", messageSize );


	return dirListMsg;
}
