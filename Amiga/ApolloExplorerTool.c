/*
 * ApolloExplorerTool.c
 *
 *  Created on: Jun 12, 2022
 *      Author: rony
 */
#include <stdio.h>
#define DBGOUT 0
#include "protocol.h"
#include "AEUtil.h"
#include "AETypes.h"
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

UBYTE *ver = "\0$VER: ApolloExplorerTool" VERSION_STRING;

typedef enum
{
	OP_LIST,
	OP_KILLALL,
	OP_NONE
} OperationType_t;

OperationType_t operationType = OP_NONE;
#define OLD_MASTER_MSGPORT_NAME "AEServerMaster"
#define ARGS_TEMPLATE "KILL/S,LIST/S"

BOOL readArguments( )
{
	LONG result[2] = { 0, 0 };

	//Get the string supplied at execution
	STRPTR cmdLineArgs = GetArgStr();
	//printf( "cmdLineArfs: (0x%08x) %s\n", cmdLineArgs, cmdLineArgs );
	if( cmdLineArgs == NULL )
	{
		printf( "Failed to get the command line arguments.\n" );
		return FALSE;
	}

	//Parse the command line arguments
	if ( ReadArgs(ARGS_TEMPLATE, result, NULL) == NULL )
	{
		printf( "Failed to read command line arguments.\n" );
		return FALSE;
	}

	//Get the arguments we need
	if( (BOOL)result[ 0 ] )
	{
		dbglog( "Operation: kill server.\n");
		operationType = OP_KILLALL;
		return TRUE;
	}
	if( (BOOL)result[ 1 ] )
	{
		dbglog( "Operation: list clients.\n");
		operationType = OP_LIST;
		return TRUE;
	}

	return FALSE;
}

void printHelp()
{
	printf( "ApolloExplorerTool - a utility that helps you manage the ApolloExplorer server on the amiga.\n" );
	printf( "KILL/S - Causes the server to shutdown\n" );
	printf( "LIST - prints a list of currently connected clients.\n" );
}

int main( int argc, char **argv )
{
	dbglog( "Starting tool.\n" );
	if( readArguments() == FALSE )
	{
		printf( "A valid argument was missing.\n" );
		printHelp();
		return 1;
	}

	//Create a message port to receive replies on
	struct MsgPort *replyMsgPort = CreateMsgPort();
	if( replyMsgPort == NULL )
	{
		printf( "Unable to crete a reply message port.\n" );
		return 1;
	}

	//Find the server's message port
	Forbid();
	struct MsgPort *msgPort = FindPort( MASTER_MSGPORT_NAME );
	Permit();
	if( msgPort == NULL )
	{

		struct MsgPort *msgPort = FindPort( OLD_MASTER_MSGPORT_NAME );
		if( msgPort == NULL )
		{
			DeleteMsgPort( replyMsgPort );
			printf( "We couldn't find the server process's message port.\n" );
			return 0;
		}else
		{
			dbglog( "Found a message port but the kill cmd will be ignored.\n" );
		}
	}

	if( operationType == OP_LIST )
	{
		//Form the message asking for a list of clients
		struct AEMessage newClientMessage;
		memset( &newClientMessage, 0, sizeof( newClientMessage ) );
		newClientMessage.msg.mn_Node.ln_Type = NT_MESSAGE;
		newClientMessage.msg.mn_Length = sizeof( struct AEMessage );
		newClientMessage.msg.mn_ReplyPort = replyMsgPort;
		newClientMessage.messageType = AEM_ClientList;
						
		dbglog( "Requesting a client list from the server.\n" );
		PutMsg( msgPort, (struct Message *)&newClientMessage );
		dbglog( "Waiting for reply.\n" );
		WaitPort( replyMsgPort );
		dbglog( "Reply received.\n" );
		struct Message *newMessage = GetMsg( replyMsgPort );

		struct AEClientList *clientListMsg = (struct AEClientList*)newMessage;

		//Double check this is a message that this is really a client list
		if( clientListMsg->msg.mn_Length < sizeof( struct AEClientList ) )
		{
			dbglog("Reply was too small!\n" );
			printf( "Invalid reply from server." );
			return 1;
		}
		if( clientListMsg->messageType != AEM_ClientList )
		{
			printf( "Invalid reply from server." );
			return 1;
		}

		//If we have an empty list, exit here
		if( clientListMsg->clientCount < 1 )
		{
			printf( "There aren't any clients connected.\n" );
			return 0;
		}

		//Now get a pointer to the start of the message
		struct clientEntry_t{
			char ipAddress[ 21 ];
			short port;
		}	*clientEntry = (struct clientEntry_t*)&clientListMsg->ipAddress;

		printf( "Client list (%u)\n", (UBYTE)clientListMsg->clientCount );
		printf( "======START======\n" );
		for( int i = 0; i < clientListMsg->clientCount; i++ )
		{
			printf( "Client: %s:%u\n", clientEntry[ i ].ipAddress, clientEntry[ i ].port );
		}
		printf( "=======END======\n" );

		//Now that we are done, free the memory for this message
		FreeVec( clientListMsg );
		
		return 0;
	}

	if( operationType == OP_KILLALL )
	{
		dbglog( "Attempting to kill the server.\n" );

		//Now send the kill message
		struct AEMessage newClientMessage = {
												.msg.mn_Node.ln_Type = NT_MESSAGE,
												.msg.mn_Length = sizeof( struct AEMessage ),
												.msg.mn_ReplyPort = replyMsgPort,
												.messageType = AEM_Shutdown,
												.port = 0
											};
		dbglog( "Signaling to server to terminate.\n" );
		PutMsg( msgPort, (struct Message *)&newClientMessage );
		dbglog( "Waiting for reply.\n" );
		WaitPort( replyMsgPort );
		dbglog( "Reply received.\n" );
		struct Message *newMessage = GetMsg( replyMsgPort );
		if( newMessage == NULL )
		{
			printf( "We got a NULL message reply.  This should never happen.\n" );
		}else
		{
			ReplyMsg( newMessage );
		}
	}

	dbglog( "Terminating.\n" );

	return 0;
}
