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
#include <stdio.h>
#include <unistd.h>
#include <sys/filio.h>
#include <proto/exec.h>
#include <clib/dos_protos.h>
#define __BSDSOCKET_NOLIBBASE__
#include <proto/bsdsocket.h>
#endif

#include "AEUtil.h"

#ifdef __VBCC__

#endif

int sendMessage( struct Library *SocketBase, SOCKET clientSocket, ProtocolMessage_t *message )
{
	dbglog( "[sendMessage] Message: token 0x%08x type 0x%08x length %d address 0x%08x\n", message->token, message->type, message->length, (unsigned int)message );

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
	unsigned int bytesLeftToSend = message->length;
	int bytesSent = 0;
	int bytesSentSoFar = 0;
	int chunkSize=4096;
	int bytesToSend=0;
	char *sendBufferPosition = (char*)message;
	while( bytesLeftToSend > 0 )
	{
		bytesToSend = chunkSize < bytesLeftToSend ? chunkSize : bytesLeftToSend;
		bytesSent = send( clientSocket, sendBufferPosition, bytesToSend, 0 );
		sendBufferPosition += bytesSent;
		dbglog( "[sendMessage] Sent %d of %d bytes from address 0x%08x.\r", bytesSent, message->length, (unsigned int)sendBufferPosition );
		if( bytesSent < 0 )
		{
			dbglog( "[sendMessage] Socket error on socket %d while sending.  Only %d bytes sent.\n", clientSocket, bytesSentSoFar );
			return SOCKET_ERROR;
		}

		//Book keeping
		bytesSentSoFar += bytesSent;
		bytesLeftToSend -= bytesSent;
	}
	dbglog( "[sendMessage] Sent %d of %d bytes.\n", bytesSentSoFar, message->length );
	return bytesSentSoFar;
}


int getMessage( struct Library *SocketBase, SOCKET clientSocket, ProtocolMessage_t *message, unsigned int maxMessageLength )
{
	int retries = 50;
	int bytesReceived = 0;
	int totalBytesReceived = 0;

	//Clear out the message
	memset( message, 0, sizeof( ProtocolMessage_t ) );

	//First get the magic token
	//dbglog( "[getMessage] Waiting on magic token bytes.\n" );
	LONG bytesAvailable = 0;
	while( retries > 0 )
	{
		IoctlSocket( clientSocket,FIONREAD ,&bytesAvailable );
		if( bytesAvailable < sizeof( ProtocolMessage_t ) )
		{
			//Delay( 1 );
			retries--;
			continue;
		}

		dbglog( "[getMessage] Reading next bytes (up to %ld ) from socket.\n", bytesAvailable );
		bytesReceived = recv( clientSocket, &message->token, sizeof( ProtocolMessage_t ), 0 );
		if( bytesReceived < sizeof( ProtocolMessage_t ) )
		{
			dbglog( "[getMessage] Failed to read all of the message header.\n" );
			return SOCKET_ERROR;	
		}else
		{
			break;
		}
	}
	dbglog( "[getMessage] Got message: Token 0x%08x   Type 0x%08x    Length %d.\n", message->token, message->type, message->length );

	//Did we fail to read any bytes?
	if( bytesReceived < sizeof( ProtocolMessage_t ) )
	{
		dbglog( "[getMessage] Failed to read all of the message header.\n" );
		return SOCKET_ERROR;	
	}

	//Now check that we got a magic token.
	if( message->token != MAGIC_TOKEN )
	{
		dbglog( "[getMessage] Didn't get magic token.\n" );
		return MAGIC_TOKEN_MISSING;
	}
	totalBytesReceived = bytesReceived;

	//sanity check
	if( message->type >= PMT_INVALID )
		return INVALID_MESSAGE_TYPE;
	if( message->length > MAX_MESSAGE_LENGTH )
		return INVALID_MESSAGE_SIZE;

	//Keep pulling bytes until we are done or there is an error
	int totalBytesLeftToRead = message->length - totalBytesReceived;
	ULONG bytesToRead = 0;
	ULONG readChunkSize = 4096;
	char *receiveBufferPosition = (char*)message + totalBytesReceived;
	while( totalBytesLeftToRead > 0 )
 	{
		bytesToRead = readChunkSize < totalBytesLeftToRead ? readChunkSize : totalBytesLeftToRead;
		dbglog( "[getMessage] Reading up to %04lu of %04d bytes, %04u bytes remaining (Adr: 0x%08x).\r", bytesToRead, totalBytesLeftToRead, message->length, (unsigned int)receiveBufferPosition );
		bytesReceived = recv( clientSocket, receiveBufferPosition, bytesToRead, MSG_WAITALL );
		receiveBufferPosition += bytesReceived;
		if( bytesReceived == -1 )
			return SOCKET_ERROR;

		totalBytesReceived += bytesReceived;
		totalBytesLeftToRead -= bytesReceived;
	}
	dbglog( "[getMessage] %d bytes of %d to read.\n", totalBytesLeftToRead, message->length );
	return totalBytesReceived;
}


int getMessageNB( struct Library *SocketBase, SOCKET clientSocket, ProtocolMessage_t *message, unsigned int maxMessageLength, fd_set *fdset, struct timeval timeOut )
{
	dbglog( "getMessage( socket(%d) 0x%08x, %d)\n", clientSocket, (unsigned int)message, maxMessageLength );
	int totalBytesReceived = 0;
	int bytesReceived = 0;
	int retries = 10;
	int totalBytesLeftToRead = 0;

	//Clear out the message
	memset( message, 0, maxMessageLength );

	FD_ZERO( fdset );
	FD_SET( clientSocket, fdset );

	//Wait on the network first
	int waitRC = WaitSelect( clientSocket+1,fdset, NULL, NULL, &timeOut, NULL );
	if( waitRC == 0 )
	{
		return TIMEOUT;
	}

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
		FD_ZERO( fdset );
		FD_SET( clientSocket, fdset );

		//Wait on the network first
		int waitRC = WaitSelect( clientSocket+1,fdset, NULL, NULL, &timeOut, NULL );
		if( waitRC == 0 )
		{
			return TIMEOUT;
		}

		dbglog( "[getMessage] %d bytes left of %d to read.\n", totalBytesLeftToRead, message->length );
		bytesReceived = recv( clientSocket, (char*)message + totalBytesReceived, totalBytesLeftToRead, 0);
		if( bytesReceived == -1 )
			return SOCKET_ERROR;

		totalBytesReceived += bytesReceived;
		totalBytesLeftToRead -= bytesReceived;
	}
	return totalBytesReceived;
}