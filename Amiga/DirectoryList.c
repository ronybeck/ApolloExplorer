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


#include "AEUtil.h"

#define DIR_MSG_LENGTH MAX_MESSAGE_LENGTH*5
ProtocolMessageDirectoryList_t *dirListMsg = NULL;

static unsigned getDirectoryNumberOfEntries( char *path, unsigned int *filenameBufferLen  )
{
	LONG returnCode = 0;
	struct AnchorPath *ap = NULL;
	unsigned int directoryListSize = 0;
	*filenameBufferLen = 0;
	dbglog( "[getDirectoryNumberOfEntries] Getting directory list for path %s.\n", path );

	//Get the entries in this path
	ap = AllocVec( sizeof( *ap ) + MAX_FILEPATH_LENGTH * 3, MEMF_FAST|MEMF_CLEAR );
	ap->ap_Strlen = MAX_FILEPATH_LENGTH;
	returnCode = MatchFirst( path, ap );
	if( returnCode )
	{
		dbglog( "[getDirectoryNumberOfEntries] We couldn't find any files in path %s.\n", path );
		MatchEnd(ap);
    	FreeVec(ap);
		return directoryListSize;
	}
	ap->ap_Flags |= APF_DODIR;
	returnCode = MatchNext( ap );
	while( returnCode == 0 )
	{
		if( strlen( ap->ap_Info.fib_FileName ) > 0 && strcmp( path, ap->ap_Buf ) )
		{
			dbglog( "[getDirectoryNumberOfEntries] Found file: %s.\n", ap->ap_Info.fib_FileName );
			directoryListSize++;
			*filenameBufferLen += strlen( ap->ap_Info.fib_FileName );
		}
		ap->ap_BreakBits = 0;
		ap->ap_Flags = 0;
		returnCode = MatchNext( ap );
	}
	MatchEnd(ap);
    FreeVec(ap);

	//Return our result
	dbglog( "[getDirectoryNumberOfEntries] There are %d entries in '%s' and a buffer of %d bytes is needed for the names.\n", directoryListSize, path, *filenameBufferLen );
	return directoryListSize;
}


ProtocolMessageDirectoryList_t *getDirectoryList( char *path )
{
	unsigned int directoryListSize = 0;
	unsigned int filenameBufferSize = 0;
	char *currentPositionInMessage = 0;
	char *endOfMessageMemory = 0;
	unsigned int sizeOfCurrentMessage = 0;
	LONG returnCode = 0;
	struct AnchorPath *ap = NULL;

	//dbglog( "[getDirectoryList] Getting directory list for path %s.\n", path );

	//get the number of entries
	directoryListSize = getDirectoryNumberOfEntries( path, &filenameBufferSize );
	dbglog( "[getDirectoryList] Directory '%s' contains %d entries and requires %d bytes to store the names.\n", path, directoryListSize, filenameBufferSize );
	if( directoryListSize == 0 )
	{
		//This path obiously doesn't exist.
		return NULL;
	}

	//The first entry we want to have contains the path we requested.  So increase our counts accordingly
	directoryListSize++;
	filenameBufferSize+=strlen( path );

	//Allocate memory for the message
	size_t messageSize = sizeof( ProtocolMessageDirectoryList_t ) + (sizeof( ProtocolMessage_DirEntry_t ) * directoryListSize ) + filenameBufferSize;
	if( dirListMsg == NULL ) dirListMsg = ( ProtocolMessageDirectoryList_t* )AllocVec( DIR_MSG_LENGTH, MEMF_FAST|MEMF_CLEAR );
	memset( dirListMsg, 0, DIR_MSG_LENGTH );
	endOfMessageMemory = ((char*)dirListMsg) + messageSize;
	dbglog( "[getDirectoryList] dirListMsg: 0x%08x\n", (unsigned int)dirListMsg );
	dbglog( "[getDirectoryList] dirListMsg size: %d bytes\n", messageSize );


	//Let's set what we already know into the message
	dirListMsg->header.token = MAGIC_TOKEN;
	dirListMsg->header.type = PMT_DIR_LIST;
	dirListMsg->header.length = messageSize;
	dirListMsg->entryCount = 1;
	currentPositionInMessage = (char*)&dirListMsg->entries[ 0 ];

	//Set the starting memory pointer for our entries
	ProtocolMessage_DirEntry_t *entry = (ProtocolMessage_DirEntry_t*)currentPositionInMessage;

	//Let's create the first message for our current directory
	entry->entrySize = sizeof( ProtocolMessage_DirEntry_t ) + strlen( path );
	strncpy( entry->filename, path, strlen( path ) );
	entry->size = 0;
	entry->type = DET_USERDIR;
	dbglog( "[getDirectoryList] entry->filename: %-30s entry->entrySize: %9u\n", entry->filename, entry->entrySize );

	//Now set the pointer for the next message
	entry = (ProtocolMessage_DirEntry_t*)((char*)entry + entry->entrySize );
	dbglog( "[getDirectoryList] Next entry address: 0x%08x\n", (unsigned int)entry );


	//Iterate through the entries until we are the end
	dbglog( "[getDirectoryList] Opening directory %s to scan\n", path );

	ap = AllocVec( sizeof( *ap ) + MAX_FILEPATH_LENGTH * 3, MEMF_FAST|MEMF_CLEAR );
	ap->ap_Strlen = MAX_FILEPATH_LENGTH;
	returnCode = MatchFirst( path, ap );
	if( returnCode )
	{
		dbglog( "[getDirectoryList] We couldn't find any files in path %s.\n", path );
		MatchEnd( ap );
		FreeVec( dirListMsg );
		FreeVec( ap );
		return NULL;
	}
	ap->ap_Flags |= APF_DODIR;
	returnCode = MatchNext( ap );
	while( returnCode == 0 )
	{
		if( strlen( ap->ap_Info.fib_FileName ) > 0 && strcmp( path, ap->ap_Buf ) )
		{
			//Now add this as an entry in our list message
			entry->size = ap->ap_Info.fib_Size;
			entry->type = ap->ap_Info.fib_EntryType;
			strncpy( entry->filename, ap->ap_Info.fib_FileName, strlen( (const char *)ap->ap_Info.fib_FileName ) );
			entry->entrySize = sizeOfCurrentMessage = sizeof( ProtocolMessage_DirEntry_t ) + strlen( entry->filename );	//Note: because of the char[1] member of the ProtocolMessage_DirEntry_t we don't need an extra byte for the'\0'
			if( ((char*)entry) + sizeOfCurrentMessage > endOfMessageMemory )
			{
				dbglog( "[getDirectoryList] Trying to write past end of message by %d ", (((char*)entry) + sizeOfCurrentMessage) - endOfMessageMemory );
				break;
			}

			//Increase the entry count
			dirListMsg->entryCount++;

			//Now let's move onto the next one
			dbglog( "[getDirectoryList] name: %-30s entrySize: %03d entryAdr 0x%08x ", entry->filename, entry->entrySize, (unsigned int)entry  );
			entry = (ProtocolMessage_DirEntry_t*)((char*)entry + entry->entrySize );
			dbglog( "NxEntryAdr 0x%08x\n", (unsigned int)entry );
		}
		ap->ap_BreakBits = 0;
		ap->ap_Flags = 0;
		returnCode = MatchNext( ap );
	}
	MatchEnd( ap );
	FreeVec( ap );
	
	dbglog( "[getDirectoryList] There are %d entries in '%s'.\n", directoryListSize, path );
	dbglog( "[getDirectoryList] Returning directory listing message of size %d bytes.\n", messageSize );


	return dirListMsg;
}
