/*
 * protocol.c
 *
 *  Created on: May 10, 2021
 *      Author: rony
 */

#include "protocol.h"

#define DBGOUT 0


#ifdef __GNUC__
#if DBGOUT
#include <stdio.h>
#endif
#include <proto/exec.h>
#define __BSDSOCKET_NOLIBBASE__
#include <proto/bsdsocket.h>
#endif

#include "VNetUtil.h"

#ifdef __VBCC__

#endif

int sendMessage( struct Library *SocketBase, SOCKET clientSocket, ProtocolMessage_t *message )
{
	dbglog( "[sendMessage] Sending message 0x%08x with size %d bytes.\n", (unsigned int)message, message->length );
	dbglog( "[sendMessage] Message token: 0x%08x\n", message->token );
	dbglog( "[sendMessage] Message type: 0x%08x\n", message->type );
	dbglog( "[sendMessage] Message length: %d\n", message->length );

	unsigned int bytesLeftToSend = message->length;
	int bytesSent = 0;
	int bytesSentSoFar = 0;

	//Sanity check
	if( message == NULL )
	{
		dbglog( "[sendMessage] Trying to send a message at an invalid pointer: 0x%08x\n", (unsigned int)message );
		return MAGIC_TOKEN_MISSING;
	}
	if( message->token != MAGIC_TOKEN )
	{
		dbglog( "[sendMessage] Invalid token: 0x%08x\n", message->token );
		return MAGIC_TOKEN_MISSING;
	}
	dbglog( "[sendMessage] message->token: 0x%08x\n", message->token );

	if( message->type > PMT_INVALID )
	{
		dbglog( "[sendMessage] Invalid message type: 0x%08x\n", message->type );
		return INVALID_MESSAGE_TYPE;
	}

	if( message->length > MAX_MESSAGE_LENGTH )
	{
		dbglog( "[sendMessage] Invalid message size: %d\n", message->length );
		return INVALID_MESSAGE_SIZE;
	}

	//start sending the data
	while( bytesLeftToSend > 0 )
	{
		bytesSent = send( clientSocket, message + bytesSentSoFar, bytesLeftToSend, 0 );
		if( bytesSent < 0 )
		{
			dbglog( "[sendMessage] Socket error on socket %d while sending.  Only %d bytes sent.\n", clientSocket, bytesSentSoFar );
			return SOCKET_ERROR;
		}

		//Book keeping
		bytesSentSoFar += bytesSent;
		bytesLeftToSend -= bytesSent;
	}

	return bytesSentSoFar;
}


int getMessage( struct Library *SocketBase, SOCKET clientSocket, ProtocolMessage_t *message, unsigned int maxMessageLength )
{
	dbglog( "getMessage( socket(%d) 0x%08x, %d)\n", clientSocket, (unsigned int)message, maxMessageLength );
	int totalBytesReceived = 0;
	int bytesReceived = 0;
	int retries = 10;
	int totalBytesLeftToRead = 0;

	//Clear out the message
	memset( message, 0, maxMessageLength );

	//First get the magic token
	dbglog( "[getMessage] Waiting on magic token bytes.\n" );
	while( retries > 0 )
	{
		dbglog( "[getMessage] Reading next bytes from socket.\n" );
		bytesReceived = recv( clientSocket, &message->token, sizeof( message->token ), 0 );
		dbglog( "[getMessage] Got %d bytes from socket: 0x%08x.\n", bytesReceived, clientSocket );
		dbglog( "[getMessage] Current token value: 0x%08x.\n", message->token );
		if( bytesReceived < sizeof( message->token ) )
		{
			dbglog( "[getMessage] Socket error while reading magic token.\n" );
			return SOCKET_ERROR;
		}
		if( message->token == MAGIC_TOKEN ) break;
		else retries--;
	}
	if( message->token != MAGIC_TOKEN && retries < 1 )
	{
		dbglog( "[getMessage] Didn't get magic token.\n" );
		return MAGIC_TOKEN_MISSING;
	}
	totalBytesReceived += bytesReceived;
	dbglog( "[getMessage] Got magic token.\n" );


	//Next get the message type
	dbglog( "[getMessage] Getting message type and size.\n" );
	bytesReceived = recv( clientSocket, &message->type, sizeof( message->type ), 0 );
	if( bytesReceived < sizeof( message->type ) )
	{
		dbglog( "[getMessage] Socket error while reading message type.\n" );
		return SOCKET_ERROR;
	}
	totalBytesReceived += bytesReceived;

	//Now get the message size
	bytesReceived = recv( clientSocket, &message->length, sizeof( message->length ), 0 );
	if( bytesReceived < sizeof( message->length ) )
	{
		dbglog( "[getMessage] Socket error while reading message type.\n" );
		return SOCKET_ERROR;
	}
	totalBytesReceived += bytesReceived;

	dbglog( "[getMessage] Got message Type 0x%08x.\n", message->type );
	dbglog( "[getMessage] Got message Length %d.\n", message->length );

	//sanity check
	if( message->type >= PMT_INVALID )
		return INVALID_MESSAGE_TYPE;
	if( message->length > MAX_MESSAGE_LENGTH )
		return INVALID_MESSAGE_SIZE;

	//Keep pulling bytes until we are done or there is an error
	totalBytesLeftToRead = message->length - totalBytesReceived;
	while( totalBytesLeftToRead > 0 )
 	{
		dbglog( "[getMessage] %d bytes left of %d to read.\n", totalBytesLeftToRead, message->length );
		bytesReceived = recv( clientSocket, (char*)message + totalBytesReceived, totalBytesLeftToRead, 0);
		if( bytesReceived == -1 )
			return SOCKET_ERROR;

		totalBytesReceived += bytesReceived;
		totalBytesLeftToRead -= bytesReceived;
	}
	return totalBytesReceived;
}
