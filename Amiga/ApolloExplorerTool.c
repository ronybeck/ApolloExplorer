/*
 * ApolloExplorerTool.c
 *
 *  Created on: Jun 12, 2022
 *      Author: rony
 */
#include <stdio.h>
#define DBGOUT 1
#include "protocol.h"
#include "AEUtil.h"
#include "AETypes.h"
#include <clib/exec_protos.h>

typedef enum
{
	OP_LIST,
	OP_KILLALL,
	OP_NONE
} OperationType_t;

#define OLD_MASTER_MSGPORT_NAME "AEServerMaster"


int main( int argc, char **argv )
{
	dbglog( "Starting tool.\n" );

	//First we need to find the main server process
	OperationType_t operationType = OP_KILLALL;
	struct Task *task = NULL;
	task = FindTask( "ApolloExplorerServerAmiga" );
	if( task == NULL )
	{
		dbglog( "Server not found.\n" );
		//return 0;
	}

	if( operationType == OP_LIST )
	{
		printf( "Found server at address: 0x%08x\n", (unsigned int)task );
		return 0;
	}

	if( operationType == OP_KILLALL )
	{
		dbglog( "Attempting to kill the server.\n" );

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
		if( msgPort == NULL )
		{

			struct MsgPort *msgPort = FindPort( OLD_MASTER_MSGPORT_NAME );
			if( msgPort == NULL )
			{
				Permit();
				DeleteMsgPort( replyMsgPort );
				printf( "We couldn't find the server process's message port.\n" );
				return 0;
			}else
			{
				dbglog( "Found a message port but the kill cmd will be ignored.\n" );
			}
		}

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
		Permit();
		dbglog( "Waiting for reply.\n" );
		WaitPort( replyMsgPort );
		dbglog( "Reply received.\n" );


	}

	return 0;
}
