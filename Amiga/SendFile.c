/*
 * FileSend.c
 *
 *  Created on: May 23, 2021
 *      Author: rony
 */

#include "SendFile.h"

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

static BPTR g_FileLock = (BPTR)NULL;
static BPTR g_FileHandle = (BPTR)NULL;
static struct FileInfoBlock g_FileInfoBlock;
static ULONG g_CurrentChunk = 0;
static ULONG g_TotalChunks = 0;

//So we don't waste to much time (re)allocating for messages we will (re)use often, we create them once
static ProtocolMessage_FileChunk_t *g_FileChunkMessage = NULL;
static ProtocolMessage_Ack_t *g_AcknowledgeMessage = NULL;
static ProtocolMessage_StartOfFileSend_t *g_StartOfFilesendMessage = NULL;

//file reading book keeping
static LONG g_TotalBytesLeftToRead = 0;
static LONG g_TotalBytesRead = 0;

ProtocolMessage_Ack_t *requestFileSend( char *path )
{
	if( g_AcknowledgeMessage == NULL )
	{
		//Prepare the ack message
		g_AcknowledgeMessage = AllocVec( sizeof( ProtocolMessage_Ack_t ), MEMF_FAST|MEMF_CLEAR );
		dbglog( "[requestFileSend] Allocated g_AcknowledgeMessage at address 0x%08x.\n", g_AcknowledgeMessage );
	}
	g_AcknowledgeMessage->header.token = MAGIC_TOKEN;
	g_AcknowledgeMessage->header.length = sizeof( ProtocolMessage_Ack_t );
	g_AcknowledgeMessage->header.type = PMT_ACK;


	//First check if this file exists
	dbglog( "[requestFileSend] Checking for the existance of '%s'.\n", path );
	dbglog( "[requestFileSend] Current g_AcknowledgeMessage address 0x%08x.\n", g_AcknowledgeMessage );
	g_FileLock = Lock( path, ACCESS_READ );
	if( g_FileLock == (BPTR)NULL )
	{
		//We couldn't get a file lock
		g_AcknowledgeMessage->response = 0;
		dbglog( "[requestFileSend] File '%s' is unreadable or doesn't exist.\n", path );
	}else
	{
		g_AcknowledgeMessage->response = 1;
		dbglog( "[requestFileSend] CFile '%s' is readable.\n", path );
		UnLock( g_FileLock );
		g_FileLock = (BPTR)NULL;
	}

	return g_AcknowledgeMessage;
}

ProtocolMessage_StartOfFileSend_t *getStartOfFileSend( char *path )
{

	if( g_StartOfFilesendMessage == NULL )
	{
		//Form our start-of-file message
		g_StartOfFilesendMessage = ( ProtocolMessage_StartOfFileSend_t* )AllocVec( sizeof( ProtocolMessage_StartOfFileSend_t ) , MEMF_FAST|MEMF_CLEAR ) + MAX_FILEPATH_LENGTH + 1;
	}
	g_StartOfFilesendMessage->header.token = MAGIC_TOKEN;
	g_StartOfFilesendMessage->header.length = sizeof( ProtocolMessage_StartOfFileSend_t ) + MAX_FILEPATH_LENGTH + 1;
	g_StartOfFilesendMessage->header.type = PMT_START_OF_SEND_FILE;

	//Reset the variable parts of the message
	strncpy( g_StartOfFilesendMessage->filePath, path, MAX_FILEPATH_LENGTH );
	g_StartOfFilesendMessage->fileSize = 0;
	g_StartOfFilesendMessage->numberOfFileChunks = 0;

	//Let's find out how big the file is first
	g_FileHandle = Open( path, MODE_OLDFILE );
	if( g_FileHandle == (BPTR)NULL )
	{
		dbglog( "[getStartOfFile] Failed to open file '%s' for reading.\n", path );
		return g_StartOfFilesendMessage;
	}

	//Examine the file in question
	if( !ExamineFH( g_FileHandle, &g_FileInfoBlock ) )
	{
		dbglog( "[getStartOfFile] Failed to examine file '%s' for reading.\n", path );
		return g_StartOfFilesendMessage;
	}

	//So how many blocks do we send?
	g_TotalChunks = g_FileInfoBlock.fib_Size / FILE_CHUNK_SIZE + ( g_FileInfoBlock.fib_Size%FILE_CHUNK_SIZE > 0 ? 1 : 0);
	g_CurrentChunk = 0;


	g_StartOfFilesendMessage->fileSize = g_FileInfoBlock.fib_Size;
	g_StartOfFilesendMessage->numberOfFileChunks = g_TotalChunks;
	dbglog( "[getStartOfFile] Filesize: %d.\n", g_StartOfFilesendMessage->fileSize );
	dbglog( "[getStartOfFile] Chunks: %d.\n", g_StartOfFilesendMessage->numberOfFileChunks );
	dbglog( "[getStartOfFile] StartOfFile Message address: 0x%08x\n", g_StartOfFilesendMessage );
	dbglog( "[getStartOfFile] StartOfFile Message token: 0x%08x\n", g_StartOfFilesendMessage->header.token );
	dbglog( "[getStartOfFile] StartOfFile Message type: 0x%08x\n", g_StartOfFilesendMessage->header.type );
	dbglog( "[getStartOfFile] StartOfFile Message length: 0x%08x\n", g_StartOfFilesendMessage->header.length );

	//We are done here
	return g_StartOfFilesendMessage;
}

ProtocolMessage_FileChunk_t *getNextFileSendChunk( char *path )
{
	int bytesRead = 0;

	//We will reserve this one time.  Then we can reuse
	if( g_FileChunkMessage == NULL )
	{
		g_FileChunkMessage = AllocVec( sizeof( ProtocolMessage_FileChunk_t ), MEMF_FAST|MEMF_CLEAR );
	}
	g_FileChunkMessage->header.token = MAGIC_TOKEN;
	g_FileChunkMessage->header.length = sizeof( ProtocolMessage_FileChunk_t );
	g_FileChunkMessage->header.type = PMT_FILE_CHUNK;

	//Clear out the current message
	memset( g_FileChunkMessage->chunk,0, FILE_CHUNK_SIZE );

	//Read the next file data
	bytesRead = Read( g_FileHandle, g_FileChunkMessage->chunk, FILE_CHUNK_SIZE );
	if( bytesRead < 0 )
	{
		//What should we do here?
		dbglog( "[getNextFileSendChunk] Reading of file '%s' failed with error code: %d.\n", path, bytesRead );
		Close( g_FileHandle );
		g_FileHandle = (BPTR)NULL;
		return NULL;
	}
	if( bytesRead == 0 )
	{
		//End-of-file
		dbglog( "[getNextFileSendChunk] Reached the end of file '%s'.\n", path );
		Close( g_FileHandle );
		g_FileHandle = (BPTR)NULL;
		return NULL;
	}
	dbglog( "[getNextFileSendChunk] Read %d bytes from file '%s'.\n", bytesRead, path );

	//Update book keeping
	g_TotalBytesLeftToRead -= bytesRead;
	g_TotalBytesRead += bytesRead;

	//Update the chunk message
	g_FileChunkMessage->bytesContained = bytesRead;
	g_FileChunkMessage->chunkNumber = g_CurrentChunk++;

	//send
	return g_FileChunkMessage;
}

void cleanupFileSend()
{
	//Close any file we have open
	if( g_FileHandle != (BPTR)NULL )
	{
		Close( g_FileHandle );
		g_FileHandle = (BPTR)NULL;
	}

	//clean up book keeping
	g_TotalBytesLeftToRead = 0;
	g_TotalBytesRead = 0;
}
