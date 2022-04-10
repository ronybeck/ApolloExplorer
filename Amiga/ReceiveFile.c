/*
 * ReceiveFile.c
 *
 *  Created on: May 23, 2021
 *      Author: rony
 */

#include "ReceiveFile.h"

#define DBGOUT 0

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
#include "protocol.h"
#include "protocolTypes.h"

static BPTR g_FileHandle = (BPTR)NULL;
static ULONG g_CurrentChunk = 0;
static ULONG g_TotalChunks = 0;
static ULONG g_FileSize = 0;
static char g_FilePath[ MAX_FILEPATH_LENGTH ];

//So we don't waste to much time (re)allocating for messages we will (re)use often, we create them once
static ProtocolMessage_Ack_t *g_AcknowledgeMessage = NULL;


ProtocolMessage_Ack_t *requestFileReceive( char *path )
{
	dbglog( "[requestFileReceive] We've been asked to recieve file '%s'\n", path );
	//If this is the first time we are calling this, then we need to allocate the message
	if( g_AcknowledgeMessage == NULL )
	{
		//Prepare the ack message
		g_AcknowledgeMessage = AllocVec( sizeof( ProtocolMessage_Ack_t ), MEMF_FAST|MEMF_CLEAR );
		g_AcknowledgeMessage->header.token = MAGIC_TOKEN;
		g_AcknowledgeMessage->header.length = sizeof( ProtocolMessage_Ack_t );
		g_AcknowledgeMessage->header.type = PMT_ACK;
	}

	//First, let's try and open a file for writing at the requested location
	dbglog( "[requestFileReceive] Attempting to open file '%s'\n", path );
	g_FileHandle = Open( path, MODE_NEWFILE );
	strncpy( g_FilePath, path, sizeof( g_FilePath ) );

	//Did we succeed?
	if( g_FileHandle )
	{
		dbglog( "[requestFileReceive] File '%s' could be opened\n", path );
		g_AcknowledgeMessage->response = 1;
	}
	else
	{
		dbglog( "[requestFileReceive] File '%s' could NOT be opened\n", path );
		g_AcknowledgeMessage->response = 0;
	}

	return g_AcknowledgeMessage;
}

void putStartOfFileReceive( ProtocolMessage_StartOfFileSend_t *startOfFilesendMessage )
{
	//Now we get the file information from the sender
	g_CurrentChunk = 0;
	g_TotalChunks = startOfFilesendMessage->numberOfFileChunks;
	g_FileSize = startOfFilesendMessage->fileSize;

	dbglog( "[putStartOfFileReceive] The file contains %d chunks and is %d bytes in size.\n", g_TotalChunks, g_FileSize );
}

int putNextFileSendChunk( ProtocolMessage_FileChunk_t *fileChunkMessage )
{
	dbglog( "[putNextFileSendChunk] filechunk number: %d\n", fileChunkMessage->chunkNumber );
	dbglog( "[putNextFileSendChunk] filechunk bytesContained: %d\n", fileChunkMessage->bytesContained );

	//Now check that we really have bytes to write
	if( fileChunkMessage->bytesContained == 0 )
	{
		//If we are not finished writting, something went wrong at the sender's end
		dbglog( "[putNextFileSendChunk] Empty file being sent?  File: '%s'.  Aborting.\n", g_FilePath );
		cleanupFileReceive();
		return 0;
	}

	//Book keeping
	g_CurrentChunk = fileChunkMessage->chunkNumber;

	//Write the contents to the disk
	dbglog( "[putNextFileSendChunk] Got file chunk %d.  Writing to file.\n", g_CurrentChunk );
	Write( g_FileHandle, fileChunkMessage->chunk, fileChunkMessage->bytesContained );

	//If this is the last chunk, close the file
	if( g_CurrentChunk == ( g_TotalChunks - 1 ) )
	{
		dbglog( "[putNextFileSendChunk] File '%s' now complete.  Closing.\n", g_FilePath );
		cleanupFileReceive();
		return 0;
	}

	//We should return the number of chunks we are still expecting
	dbglog( "[putNextFileSendChunk] Current chunk %d.  Total chunks %d.  Chunks remaining %d\n", g_CurrentChunk, g_TotalChunks, g_TotalChunks - g_CurrentChunk -1 );
	return g_TotalChunks - g_CurrentChunk -1;
}

void cleanupFileReceive()
{
	dbglog( "[cleanupFileReceive] Cleaning up for file '%s'.\n", g_FilePath );

	//Close the file if it is open
	if( g_FileHandle )
	{
		Close( g_FileHandle );
		g_FileHandle = (BPTR)NULL;
	}

	//Reset globals
	g_FileSize = 0;
	g_CurrentChunk = 0;
	g_TotalChunks = 0;
	memset( g_FilePath, 0, sizeof( g_FilePath ) );
}
