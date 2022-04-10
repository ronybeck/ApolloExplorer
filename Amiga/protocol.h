/*
 * protocol.h
 *
 *  Created on: May 10, 2021
 *      Author: rony
 */

#ifndef AMIGA_PROTOCOL_H_
#define AMIGA_PROTOCOL_H_

#include "../protocolTypes.h"

#ifdef __GNUC__
#include <sys/unistd.h>
#include <proto/bsdsocket.h>
#endif

#ifdef __VBCC__

#endif

#define MASTER_MSGPORT_NAME "VNetServerMaster"

typedef int SOCKET;

//Error codes for the socket
#define MAGIC_TOKEN_MISSING -1
#define INVALID_MESSAGE_TYPE -2
#define INVALID_MESSAGE_SIZE -3
#define SOCKET_ERROR -4

int sendMessage( struct Library *SocketBase, SOCKET clientSocket, ProtocolMessage_t *message );
int getMessage( struct Library *SocketBase, SOCKET socket, ProtocolMessage_t *message, unsigned int maxMessageLength );

#endif /* AMIGA_PROTOCOL_H_ */

