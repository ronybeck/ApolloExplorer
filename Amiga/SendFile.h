/*
 * FileSend.h
 *
 *  Created on: May 23, 2021
 *      Author: rony
 */

#ifndef AMIGA_SENDFILE_H_
#define AMIGA_SENDFILE_H_

#include "protocolTypes.h"

ProtocolMessage_Ack_t *requestFileSend( char *path );

ProtocolMessage_StartOfFileSend_t *getStartOfFileSend( char *path );

ProtocolMessage_FileChunk_t *getNextFileSendChunk( char *path );

void cleanupFileSend();

#endif /* AMIGA_SENDFILE_H_ */
