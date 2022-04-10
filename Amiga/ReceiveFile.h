/*
 * ReceiveFile.h
 *
 *  Created on: May 23, 2021
 *      Author: rony
 */

#ifndef AMIGA_RECEIVEFILE_H_
#define AMIGA_RECEIVEFILE_H_

#include "protocolTypes.h"

ProtocolMessage_Ack_t *requestFileReceive( char *path );

void putStartOfFileReceive( ProtocolMessage_StartOfFileSend_t *startOfFilesendMessage );

int putNextFileSendChunk( ProtocolMessage_FileChunk_t *fileChunkMessage );

void cleanupFileReceive();

#endif /* AMIGA_RECEIVEFILE_H_ */
