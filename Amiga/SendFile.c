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

#include "AEUtil.h"
#include "protocol.h"
#include "protocolTypes.h"

#if 0
static BPTR g_FileLock = (BPTR)NULL;
static BPTR g_FileHandle = (BPTR)NULL;
static struct FileInfoBlock g_FileInfoBlock;
static ULONG g_CurrentChunk = 0;
static ULONG g_TotalChunks = 0;

//file reading book keeping
static LONG g_TotalBytesLeftToRead = 0;
static LONG g_TotalBytesRead = 0;

//So we don't waste to much time (re)allocating for messages we will (re)use often, we create them once
static ProtocolMessage_FileChunk_t *g_FileChunkMessage = NULL;
static ProtocolMessage_Ack_t *g_AcknowledgeMessage = NULL;
static ProtocolMessage_StartOfFileSend_t *g_StartOfFilesendMessage = NULL;
#endif




ProtocolMessage_Ack_t *requestFileSend( char *path, FileSendContext_t *context )
{
	//Valid context?
	if( context == NULL )
	{
		//We are currently doing a file send
		dbglog( "Invalid context for file send!\n" );
		return NULL;
	}

	//Set up the message
	context->acknowledgeMessage->header.token = MAGIC_TOKEN;
	context->acknowledgeMessage->header.length = sizeof( ProtocolMessage_Ack_t );
	context->acknowledgeMessage->header.type = PMT_ACK;

	//First check if this file exists
	dbglog( "[requestFileSend] Checking for the existance of '%s'.\n", path );
	dbglog( "[requestFileSend] Current g_AcknowledgeMessage address 0x%08x.\n", context->acknowledgeMessage );
	context->fileLock = Lock( path, ACCESS_READ );
	if( context->fileLock == (BPTR)NULL )
	{
		//We couldn't get a file lock
		context->acknowledgeMessage->response = 0;
		dbglog( "[requestFileSend] File '%s' is unreadable or doesn't exist.\n", path );
	}else
	{
		context->acknowledgeMessage->response = 1;
		dbglog( "[requestFileSend] CFile '%s' is readable.\n", path );
		UnLock( context->fileLock );
		context->fileLock = (BPTR)NULL;
	}

	dbglog( "[requestFileSend] Acknowledge response set to %d.\n", context->acknowledgeMessage->response );

	return context->acknowledgeMessage;
}

ProtocolMessage_StartOfFileSend_t *getStartOfFileSend( char *path, FileSendContext_t *context )
{
	//Do we have a valid context?
	if( context == NULL )
	{
		dbglog( "Invalid context.  Shame the user will never know.\n" );
		return NULL;
	}

	//Reset the variable parts of the message
	strncpy( context->startOfFilesendMessage->filePath, path, MAX_FILEPATH_LENGTH );
	context->startOfFilesendMessage->fileSize = 0;
	context->startOfFilesendMessage->numberOfFileChunks = 0;

	//Let's find out how big the file is first
	context->fileHandle = Open( path, MODE_OLDFILE );
	if( context->fileHandle == (BPTR)NULL )
	{
		dbglog( "[getStartOfFile] Failed to open file '%s' for reading.\n", path );
		return context->startOfFilesendMessage;
	}

	//Examine the file in question
	if( !ExamineFH( context->fileHandle, &context->fileInfoBlock ) )
	{
		dbglog( "[getStartOfFile] Failed to examine file '%s' for reading.\n", path );
		return context->startOfFilesendMessage;
	}

	//So how many blocks do we send?
	context->totalChunks = context->fileInfoBlock.fib_Size / FILE_CHUNK_SIZE + ( context->fileInfoBlock.fib_Size%FILE_CHUNK_SIZE > 0 ? 1 : 0);
	context->currentChunk = 0;


	context->startOfFilesendMessage->fileSize = context->fileInfoBlock.fib_Size;
	context->startOfFilesendMessage->numberOfFileChunks = context->totalChunks;
	dbglog( "[getStartOfFile] Filesize: %d.\n", context->startOfFilesendMessage->fileSize );
	dbglog( "[getStartOfFile] Chunks: %d.\n", context->startOfFilesendMessage->numberOfFileChunks );
	dbglog( "[getStartOfFile] StartOfFile Message address: 0x%08x\n", (int)context->startOfFilesendMessage );
	dbglog( "[getStartOfFile] StartOfFile Message token: 0x%08x\n", context->startOfFilesendMessage->header.token );
	dbglog( "[getStartOfFile] StartOfFile Message type: 0x%08x\n", context->startOfFilesendMessage->header.type );
	dbglog( "[getStartOfFile] StartOfFile Message length: 0x%08x\n", context->startOfFilesendMessage->header.length );

	//We are done here
	return context->startOfFilesendMessage;
}

ProtocolMessage_FileChunk_t *getNextFileSendChunk( char *path, FileSendContext_t *context )
{
	int bytesRead = 0;

	//Valid context?
	if( context == NULL )
	{
		dbglog( "Invalid context.  Shame the user will never know.\n" );
		return NULL;
	}

	//Clear out the current message
	memset( context->fileChunkMessage->chunk,0, FILE_CHUNK_SIZE );

	//Read the next file data
	bytesRead = Read( context->fileHandle, context->fileChunkMessage->chunk, FILE_CHUNK_SIZE );
	if( bytesRead < 0 )
	{
		//What should we do here?
		dbglog( "[getNextFileSendChunk] Reading of file '%s' failed with error code: %d.\n", path, bytesRead );
		Close( context->fileHandle );
		context->fileHandle = (BPTR)NULL;
		return NULL;
	}
	if( bytesRead == 0 )
	{
		//End-of-file
		dbglog( "[getNextFileSendChunk] Reached the end of file '%s'.\n", path );
		Close( context->fileHandle );
		context->fileHandle = (BPTR)NULL;
		return NULL;
	}
	dbglog( "[getNextFileSendChunk] Read %d bytes from file '%s'.\n", bytesRead, path );

	//Update book keeping
	context->totalBytesLeftToRead -= bytesRead;
	context->totalBytesRead += bytesRead;

	//Update the chunk message
	context->fileChunkMessage->bytesContained = bytesRead;
	context->fileChunkMessage->chunkNumber = context->currentChunk++;

	//send
	return context->fileChunkMessage;
}

void cleanupFileSend( FileSendContext_t *context )
{
	//Valid context?
	if( context == NULL ) return;

	//Close any file we have open
	if( context->fileHandle != (BPTR)NULL )
	{
		Close( context->fileHandle );
		context->fileHandle = (BPTR)NULL;
	}

	//clean up book keeping
	context->totalBytesLeftToRead = 0;
	context->totalBytesRead = 0;
	context->currentChunk = 0;
	context->totalChunks = 0;
}

FileSendContext_t *allocateFileSendContext()
{
	FileSendContext_t *context = (FileSendContext_t*)AllocVec( sizeof( FileSendContext_t ), MEMF_CLEAR|MEMF_FAST );

	//Allocate the achknowledge message
	context->acknowledgeMessage = AllocVec( sizeof( ProtocolMessage_Ack_t ), MEMF_FAST|MEMF_CLEAR );
	context->acknowledgeMessage->header.token = MAGIC_TOKEN;
	context->acknowledgeMessage->header.length = sizeof( ProtocolMessage_Ack_t );
	context->acknowledgeMessage->header.type = PMT_ACK;

	//Allocate the start of send message
	context->startOfFilesendMessage = ( ProtocolMessage_StartOfFileSend_t* )AllocVec( MAX_MESSAGE_LENGTH , MEMF_FAST|MEMF_CLEAR ) + MAX_FILEPATH_LENGTH + 1;
	context->startOfFilesendMessage->header.token = MAGIC_TOKEN;
	context->startOfFilesendMessage->header.length = sizeof( ProtocolMessage_StartOfFileSend_t ) + MAX_FILEPATH_LENGTH + 1;
	context->startOfFilesendMessage->header.type = PMT_START_OF_SEND_FILE;

	//Allocate the file chunk message
	context->fileChunkMessage = AllocVec( sizeof( ProtocolMessage_FileChunk_t ), MEMF_FAST|MEMF_CLEAR );
	context->fileChunkMessage->header.token = MAGIC_TOKEN;
	context->fileChunkMessage->header.length = sizeof( ProtocolMessage_FileChunk_t );
	context->fileChunkMessage->header.type = PMT_FILE_CHUNK;

	return context;
}

void freeFileSendContext( FileSendContext_t *context )
{
	//Is it a valid pointer?
	if( context == NULL )	return;

	//Make sure we have cleaned up after ourselves
	cleanupFileSend( context );

	//Free everything
	FreeVec( context->acknowledgeMessage );
	FreeVec( context->startOfFilesendMessage );
	FreeVec( context->fileChunkMessage );
}
