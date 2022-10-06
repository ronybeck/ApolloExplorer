#define DBGOUT 1

#include <sys/types.h>
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>
#include <sys/ioctl.h>

#include "protocolTypes.h"
#include "protocol.h"
#include "AETypes.h"
#include "AEUtil.h"
#include "AEClientThread.h"
#include "AEDiscoveryThread.h"

UBYTE *ver = "\0$VER: ApolloExplorerSrv " VERSION_STRING;

struct Library *SocketBase = NULL;

/*****************************************************************************/

#include <proto/exec.h>
#define __BSDSOCKET_NOLIBBASE__
#include <proto/bsdsocket.h>

char g_KeepServerRunning = 1;

char* g_MessagePortName __attribute((aligned(4))) = MASTER_MSGPORT_NAME;
struct MsgPort *g_AEServerMessagePort = NULL;

/*****************************************************************************/

int main(int argc, char *argv[])
{
	struct sockaddr_in addr __attribute__((aligned(4)));
	//SOCKET s = 0;
	SOCKET serverSocket __attribute__((aligned(4))) = 0;
	int res = 0;

	//We want to do at least a few attempts to start this
	//Maybe the TCP/Stack is starting in the back ground
	//and we need to give it a few seconds to start
	int numberOfRestartAttempts = 15;

	//Initialise the client thread list
	initialiseClientThreadList();

	while( numberOfRestartAttempts-- > 0 )
	{
		dbglog( "Opening bsdsocket.library.\n" );
		if( (SocketBase = OpenLibrary("bsdsocket.library", 4 )))
			break;
		dbglog( "No TCP/IP Stack running!\n" );
		dbglog( "Will retry in 2 seconds.\n" );
		Delay( 100 );
	}

	//Did we open the bsdsocket library successfully?
	if( !SocketBase )
	{
		dbglog( "Failed to open bsdsocket.library.  Is the TCP Stack running?.\n" );
		return 1;
	}

	dbglog( "Opening socket.\n" );
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(serverSocket == SOCKET_ERROR)
	{
		dbglog( "socket error\n" );
		return 1;
	}

	dbglog( "Setting re-useaddress socket options.\n" );
	int yes = 1;
	res = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	if(res == SOCKET_ERROR)
	{
		dbglog("setsockopt error\n");
		return 1;
	}

	//Set non-blocking on the server socket
	yes = 1;
	dbglog( "Setting non-blocking socket options.\n" );
	if( IoctlSocket( serverSocket, FIONBIO, &yes) < 0 )
	{
		dbglog( "Unable to set non-blocking on the server socket.\n" );
	}


	dbglog( "Binding to port %d\n", MAIN_LISTEN_PORTNUMBER );
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(MAIN_LISTEN_PORTNUMBER);
	res = bind(serverSocket, (struct sockaddr *)&addr, sizeof(addr));
	if(res == SOCKET_ERROR)
	{
		dbglog( "bind error\n" );
		return 1;
	}

	dbglog( "Listening on port.\n" );
	res = listen(serverSocket, 10);
	if(res == SOCKET_ERROR)
	{
		dbglog( "listen error\n" );
		return 1;
	}

	//We need to be able to check both the network socket and message port for incoming messages
	//So we need to setup WaitSelect();
	fd_set networkReadSet __attribute__((aligned(4)));
	struct timeval timeOut __attribute__((aligned(4))) = { .tv_sec = 1, .tv_usec = 0};	//1s
	FD_ZERO( &networkReadSet );
	FD_SET( serverSocket, &networkReadSet );

	//Create a message port so we can send the socket handles to child processes
	dbglog( "[master] Creating message port %s.\n", g_MessagePortName );
	g_AEServerMessagePort = CreateMsgPort();
	if( g_AEServerMessagePort == NULL )
	{
		dbglog( "Failed to create message port.\n" );
		goto shutdown;
	}
	g_AEServerMessagePort->mp_Node.ln_Name = g_MessagePortName;
	g_AEServerMessagePort->mp_Node.ln_Pri = 0;
	AddPort( g_AEServerMessagePort );
	dbglog( "[master] Master Message Port: 0x%08x\n", (unsigned int)g_AEServerMessagePort );

	//Start announcing our presence on the network
	startDiscoveryThread();

	//Start listening for new connections
	while( g_KeepServerRunning )
	{
		//We don't want 100% CPU usage
		Delay( 5 );

		//dbglog("Awaiting new connection\n");
		FD_ZERO( &networkReadSet );
		FD_SET( serverSocket, &networkReadSet );

		//Wait on the network first
		int waitRC = WaitSelect( serverSocket+1,&networkReadSet, NULL, NULL, &timeOut, NULL );

		if( waitRC > 0 )
		{
			//Any new client connection?
			socklen_t addrLen __attribute__((aligned(4))) = sizeof( addr );
			SOCKET newClientSocket = (SOCKET)accept(serverSocket, (struct sockaddr *)&addr, &addrLen);
			if( newClientSocket < 0 )
			{
				dbglog( "accept error %d \"%s\"\n", errno, strerror( errno ) );
				continue;
			}else
			{
				//Now spawn a client thread
				dbglog( "[master] Starting client thread for new socket 0x%08x\n", newClientSocket );
				startClientThread( SocketBase, g_AEServerMessagePort, newClientSocket );
				dbglog( "[master] New client connection.\n" );
			}
		}


		//Anything on the message port waiting?
		//dbglog( "Checking message port.\n" );
		struct Message *newMessage = GetMsg( g_AEServerMessagePort );
		if( newMessage != NULL )
		{
			//If this is a terminate message, start shutting down the clients
			struct AEMessage *aeMsg = (struct AEMessage *)newMessage;
			if( aeMsg->messageType == AEM_Shutdown )
			{
				dbglog( "[master] Got a shutdown message.\n" );

				//Kill the discovery thread
				Forbid();
				struct MsgPort *discoveryMsgPort = FindPort( DISCOVERY_MESSAGE_PORT_NAME );
				Permit();
				if( discoveryMsgPort )
				{
					dbglog( "[master] Signalling to the discovery thread to terminate.\n" );
					struct AEMessage killDiscoveryMessage;
					memset( &killDiscoveryMessage, 0, sizeof( killDiscoveryMessage ) );
					killDiscoveryMessage.messageType = AEM_Shutdown;
					killDiscoveryMessage.msg.mn_Length = sizeof( aeMsg );
					killDiscoveryMessage.msg.mn_ReplyPort = g_AEServerMessagePort;
					killDiscoveryMessage.msg.mn_Node.ln_Type = NT_MESSAGE;
					PutMsg( discoveryMsgPort, (struct Message*)&killDiscoveryMessage );
					dbglog( "[master] Signal sent.  Awaiting acknowledgment.\n" );
					WaitPort( g_AEServerMessagePort );	//Wait for the discovery thread to acknowledge termination
					dbglog( "[master] Acknowledgment received.\n" );
					GetMsg( g_AEServerMessagePort );	//Pop the message from the queue
				}else{
					dbglog( "[master] Failed to find the discovery thread's message port.\n" );
				}

				//Kill the clients now
				//First get the list of clients
				dbglog( "[master] Getting the list of clients.\n" );
				lockClientThreadList();
				ClientThread_t *clientList = getClientThreadList();
				unlockClientThreadList();
				dbglog( "[master] Signalling each client to terminate.\n" );
				ClientThread_t *client = clientList->next;
				while( client->next )
				{	
					//Send a signal to the client thread to terminate
					char ipAddress[17] = "";
					getIPFromClient( client, ipAddress );
					dbglog( "[master] Forming kill message to process 0x%08x for client %s:%u\n",
											(unsigned int)client->process,
											ipAddress,
											client->port );
					struct AEMessage terminateMessage __attribute__((aligned(4)));
					memset( &terminateMessage, 0, sizeof( terminateMessage ) );
					terminateMessage.messageType = AEM_KillClient;
					terminateMessage.msg.mn_ReplyPort = g_AEServerMessagePort;
					terminateMessage.msg.mn_Node.ln_Type = NT_MESSAGE;
					terminateMessage.msg.mn_Length = sizeof( struct AEMessage );
					dbglog( "[master] Sending kill message to client process.\n" );
					PutMsg( client->messagePort, (struct Message*)&terminateMessage );
					dbglog( "[master] Awaiting acknowledgment from client process.\n" );
					WaitPort( g_AEServerMessagePort );	//Wait for the client to acknowledge termination
					dbglog( "[master] Acknowledgement recieved from client process.\n" );
					GetMsg( g_AEServerMessagePort );	//Pop the message from the queue
					client = client->next;
				}
				dbglog( "[master] Freeing client list.\n" );
				freeClientThreadList( clientList );

				//Give the clients time to remove themselves from the client list
				dbglog( "[master] Giving clients time to remove themselves from the client list.\n" );
				Delay( 10 );

				//Send a reply to the caller
				dbglog( "[master] Acknowledging the caller's shutdown request.\n" );
				newMessage->mn_Node.ln_Type = NT_REPLYMSG;
				ReplyMsg( newMessage );
				
				//Now exit
				dbglog( "[master] Starting shutdown.\n" );
				goto shutdown;
			}else if( aeMsg->messageType == AEM_ClientList )
			{
				dbglog( "[master] Got a request for the client list.\n" );

				//Get the client list
				dbglog( "[master] Lock the client list.\n" );
				lockClientThreadList();
				dbglog( "[master] Count the clients.\n" );
				UBYTE clientCount = getClientListSize();
				dbglog( "[master] Client count: %d.\n", clientCount );

				//Allocate the bytes needed to store this.
				dbglog( "[master] Form the reply message.\n" );
				LONG messageSize = sizeof( struct AEClientList ) * clientCount;	//Bigger than necessary but keeps the maths simple.
				struct AEClientList *clientListMessage = (struct AEClientList *)AllocVec( messageSize, MEMF_FAST|MEMF_CLEAR );
				clientListMessage->msg.mn_Length = messageSize;
				clientListMessage->msg.mn_Node.ln_Type = NT_MESSAGE;
				clientListMessage->msg.mn_Node.ln_Pri = 0;
				clientListMessage->msg.mn_ReplyPort = g_AEServerMessagePort;
				clientListMessage->messageType = AEM_ClientList;
				clientListMessage->clientCount = clientCount;

				//Get a pointer to the start of the list in the message
				dbglog( "[master] Inserting entries into the list.\n" );
				struct ClientEntry_t
				{
					char ipAddress[ 21 ];
					short port;
				} *clientEntry = (struct ClientEntry_t*)&clientListMessage->ipAddress;

				//Populate the list
				ClientThread_t *node = getClientThreadList()->next;
				int index = 0;
				while( node->next )
				{
					snprintf( clientEntry[ index ].ipAddress, sizeof( clientEntry[ index ].ipAddress ), "%u.%u.%u.%u",
					(UBYTE)node->ip[ 0 ],
					(UBYTE)node->ip[ 1 ],
					(UBYTE)node->ip[ 2 ],
					(UBYTE)node->ip[ 3 ] );
					clientEntry[ index ].port = node->port;
					dbglog( "[master] Added entry %s:%u.\n", clientEntry[ index ].ipAddress, clientEntry[ index ].port );
					index++;
					node = node->next;
				}
				dbglog( "[master] Unlocking the list.\n" );
				unlockClientThreadList();
				PutMsg( newMessage->mn_ReplyPort, (struct Message*)clientListMessage );
			}else{
				dbglog( "[master] We got an unknown message (0x%08x) on the message port.\n", (unsigned int)aeMsg->messageType );
			}
		}
	}

	shutdown:
	dbglog( "[master] Shutting down.\n" );
	Delay( 10 );

	//Clear out and then free the message port
	dbglog( "[master] Removing message port.\n" );

	RemPort( g_AEServerMessagePort );
	DeleteMsgPort( g_AEServerMessagePort );

	//Shutdown the socket.
	dbglog( "[master] Closing socket.\n" );
	CloseSocket(serverSocket);

	dbglog( "[master] Terminating.\n" );
	return 0;
}




