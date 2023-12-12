/*
 * FileSend.h
 *
 *  Created on: May 23, 2021
 *      Author: rony
 */

#ifndef AMIGA_SENDFILE_H_
#define AMIGA_SENDFILE_H_

#include "protocolTypes.h"
#include <proto/dos.h>
#include <proto/exec.h>

typedef struct
{
	//File handling
	BPTR fileLock;
	BPTR fileHandle;
	struct FileInfoBlock fileInfoBlock;
	ULONG currentChunk;
	ULONG totalChunks;

	//file reading book keeping
	LONG totalBytesLeftToRead;
	LONG totalBytesRead;

	//Protocol messages we will use
	ProtocolMessage_FileChunk_t *fileChunkMessage;
	ProtocolMessage_Ack_t *acknowledgeMessage;
	ProtocolMessage_StartOfFileSend_t *startOfFilesendMessage;
} FileSendContext_t;


ProtocolMessage_Ack_t *requestFileSend( char *path, FileSendContext_t *context );

ProtocolMessage_StartOfFileSend_t *getStartOfFileSend( char *path, FileSendContext_t *context );

ProtocolMessage_FileChunk_t *getNextFileSendChunk( char *path, FileSendContext_t *context );

void cleanupFileSend( FileSendContext_t *context );

FileSendContext_t *allocateFileSendContext();

void freeFileSendContext( FileSendContext_t *context );

#endif /* AMIGA_SENDFILE_H_ */
