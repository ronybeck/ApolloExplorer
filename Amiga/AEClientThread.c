/*
 * AEClientThread.c
 *
 *  Created on: May 16, 2021
 *      Author: rony
 */

#define DBGOUT 0

#ifdef __GNUC__
#include <stdio.h>
#include <unistd.h>
#include <sys/filio.h>
#include <netinet/tcp.h>
#include <proto/dos.h>
#include <clib/exec_protos.h>
//#include <clib/timer_protos.h>
#include <inline/timer.h>
#include <dos/dostags.h>
#define __BSDSOCKET_NOLIBBASE__
#include <proto/bsdsocket.h>
#endif

#ifdef __VBCC__

#endif

#include "AEClientThread.h"
#include "AETypes.h"
#include "AEUtil.h"
#include "DirectoryList.h"
#include "SendFile.h"
#include "ReceiveFile.h"
#include "MakeDir.h"
#include "VolumeList.h"
#include "DeletePath.h"


extern char g_KeepServerRunning;
extern volatile LONG g_ActiveThreadCount;

static unsigned short g_NextClientPort = MAIN_LISTEN_PORTNUMBER + 1;

static void clientThread();
static void clientThreadBody();

ClientThread_t *g_ClientThreadList;
struct SignalSemaphore g_ClientThreadListLock;

void initialiseClientThreadList()
{
	//Initialise and then obtain the semaphore, so we won't be disturbed
	InitSemaphore( &g_ClientThreadListLock );
	lockClientThreadList();

	//Initialise the list.  This is a doubly linked list with NULL objects at each end
	ClientThread_t *head = AllocVec( sizeof( ClientThread_t ), MEMF_CLEAR|MEMF_FAST );
	ClientThread_t *tail = AllocVec( sizeof( ClientThread_t ), MEMF_CLEAR|MEMF_FAST );
	head->next = tail;
	tail->previous = head;
	g_ClientThreadList = head;

	//We are done
	unlockClientThreadList();
}

void freeClientThreadList( ClientThread_t *list )
{
	ClientThread_t *head = list;
	ClientThread_t *node = head->next;
	while( node->next )
	{
		ClientThread_t *nextNode = node->next;
		FreeVec( node );
		node = nextNode;
	}
	FreeVec( node );	//This should be the tail
	FreeVec( head );
}

//Free the global client list itself (the head/tail sentinels allocated in
//initialiseClientThreadList).  Call once at shutdown, after all clients are gone.
void destroyClientThreadList()
{
	lockClientThreadList();
	if( g_ClientThreadList != NULL )
	{
		freeClientThreadList( g_ClientThreadList );
		g_ClientThreadList = NULL;
	}
	unlockClientThreadList();
}

void addClientThreadToList( ClientThread_t *client )
{
	//Lock the list
	lockClientThreadList();

	//Add this to the front of the list
	ClientThread_t *head = g_ClientThreadList;
	ClientThread_t *first = head->next;
	client->previous = head;
	client->next = first;
	head->next = client;
	first->previous = client;

	//Unlock the list
	unlockClientThreadList();
}

void removeClientThreadFromList( ClientThread_t *client )
{
	//Lock the list
	lockClientThreadList();

	//Find the entry in question
	ClientThread_t *node = g_ClientThreadList->next;
	while( node->next )
	{
		ClientThread_t *nextNode = node->next;
		ClientThread_t *previousNode = node->previous;
		if( node == client || node->process == client->process )
		{
			//We found the node in question.
			//Relink the nodes before and after this node with each other
			nextNode->previous = previousNode;
			previousNode->next = nextNode;
			client->next = NULL;
			client->previous = NULL;

			//Free the unlinked list entry.  It was AllocVec'd per connection in
			//startClientThread(); without this it leaks on every disconnect and the
			//memory is never reclaimed (AmigaOS does not free a process's
			//allocations on exit).
			FreeVec( node );

			unlockClientThreadList();
			return;
		}

		node = node->next;
	}

	//If we made it here, then we never found the client thread.
	dbglog( "[%s] We didn't find this client thread.\n", __FUNCTION__ );
	unlockClientThreadList();
}

ClientThread_t *getClientThreadList()
{
	//Make a copy and return that
	ClientThread_t *head = AllocVec( sizeof( ClientThread_t ), MEMF_FAST|MEMF_CLEAR );
	ClientThread_t *tail = AllocVec( sizeof( ClientThread_t ), MEMF_FAST|MEMF_CLEAR );
	head->next = tail;
	tail->previous = head;

	ClientThread_t *node = g_ClientThreadList->next;
	while( node->next )
	{
		//Insert the new node
		ClientThread_t *nodeCopy = AllocVec( sizeof( ClientThread_t ), MEMF_FAST|MEMF_CLEAR );
		nodeCopy->next = head->next;
		head->next->previous = nodeCopy;
		nodeCopy->previous = head;
		head->next = nodeCopy;

		//Copy the contents
		memcpy( nodeCopy->ip, node->ip, sizeof( node->ip ) );
		nodeCopy->port = node->port;
		nodeCopy->messagePort = node->messagePort;

		//Next thread
		node = node->next;
	}
	return head;
}

void lockClientThreadList()
{
	ObtainSemaphore( &g_ClientThreadListLock );
}

void unlockClientThreadList()
{
	ReleaseSemaphore( &g_ClientThreadListLock );
}

static void removeClientByPort( UWORD port )
{
	//Traverse the list and find it in the list
	ClientThread_t *node = g_ClientThreadList->next;
	while( node->next )
	{
		if( node->port == port )
		{
			dbglog( "[?] Removed client with port allocation %u from client list.\n", port );
			removeClientThreadFromList( node );
			return;
		}
		node = node->next;
	}
}
#if 0
static void removeClientBySocket( SOCKET clientSocket )
{
		//Get the peer name
		struct sockaddr_in name;
		socklen_t nameLen = sizeof( name );
		getpeername( clientSocket, (struct sockaddr *)&name, &nameLen );
		UWORD port = name.sin_port;

		//Now remove by port
		removeClientByPort( port );
}
#endif

UBYTE getClientListSize()
{
	UBYTE count = 0;
	ClientThread_t *node = g_ClientThreadList->next;
	while( node->next )	{	count++; node = node->next;	}
	return count;
}

void getIPFromClient( ClientThread_t *client, char ipAddress[ 17 ] )
{
	//We assume that the caller is smart enough to allocate the required bytes in the string
	snprintf( ipAddress, 17, "%u.%u.%u.%u", 
					(UBYTE)client->ip[0], 
					(UBYTE)client->ip[1],
					(UBYTE)client->ip[2],
					(UBYTE)client->ip[3] );
}

void startClientThread( struct Library *SocketBase, struct MsgPort *msgPort, SOCKET clientSocket )
{
	dbglog( "[master] Accepted socket connection.\n" );
	if ( DOSBase )
	{
		//Start a client thread
		struct TagItem tags[] = {
				{ NP_StackSize,		16384 },
				{ NP_Name,			(ULONG)"AEClientThread" },
				{ NP_Entry,			(ULONG)clientThread },
				{ NP_Priority,		(ULONG)8 },
				//{ NP_Output,		(ULONG)consoleHandle },
				{ NP_Synchronous, 	FALSE },
				{ TAG_DONE, 0UL }
		};
		dbglog( "[master] Starting client thread.\n" );
		struct Process *clientProcess = CreateNewProc(tags);
		if( clientProcess == NULL )
		{
			dbglog( "[master] Failed to create client thread.\n" );
			CloseSocket( clientSocket );
			//CloseLibrary( DOSBase );
			return;
		}
		//Count this live child so the master waits for it before unloading the seglist.
		Forbid(); g_ActiveThreadCount++; Permit();
		//dbglog( "[master] Child started.\n" );

			//Wait (bounded) for the child to tell us its listen port.  A child that dies
			//during its own setup must NOT wedge the master here forever (that made the
			//server impossible to kill), and a shutdown request that lands on this same
			//shared port while we wait must still be honoured.
			dbglog( "[master] Waiting for child to tell us what port the client should reconnect on.\n" );
			struct Message *clientMessage = NULL;
			struct AEMessage *newClientMessage = NULL;
			LONG regTimeout = 250;	//~5s at Delay(2)
			while( regTimeout-- > 0 )
			{
				clientMessage = GetMsg( msgPort );
				if( clientMessage != NULL )
				{
					newClientMessage = (struct AEMessage *)clientMessage;

					//Shutdown can arrive on this shared port while we are mid-registration.
					if( newClientMessage->messageType == AEM_Shutdown
					 || newClientMessage->messageType == WB_ICON_DOUBLE_CLICKED )
					{
						dbglog( "[master] Shutdown received while registering a client.  Honouring it.\n" );
						g_KeepServerRunning = 0;
						newClientMessage->msg.mn_Node.ln_Type = NT_REPLYMSG;
						ReplyMsg( clientMessage );
						CloseSocket( clientSocket );
						return;
					}
					break;	//Got the child's AEM_NewClient / AEM_KillClient
				}
				Delay( 2 );
			}
			if( newClientMessage == NULL )
			{
				dbglog( "[master] Child did not register within timeout.  Abandoning connection.\n" );
				CloseSocket( clientSocket );
				return;
			}
			unsigned short clientPort = newClientMessage->port;

		//Did the child request to kill the client?
		if( newClientMessage->messageType == AEM_KillClient )
		{
			dbglog( "[master] Child requested the termination of new client.\n" );
			CloseSocket( clientSocket );
			ReplyMsg( (struct Message*)newClientMessage );
			dbglog( "[master] Reply to child.\n" );
			return;
		}
		dbglog( "[master] Got the listen port for the new client %d.\n", clientPort );

		//Send a reply
		ReplyMsg( (struct Message*)newClientMessage );
		dbglog( "[master] Reply to child.\n" );


		//Inform the client on which port they should now connect
		ProtocolMessageNewClientPort_t reconnectMessage;
		reconnectMessage.header.token = MAGIC_TOKEN;
		reconnectMessage.header.length = sizeof( reconnectMessage );
		reconnectMessage.header.type = PMT_NEW_CLIENT_PORT;
		reconnectMessage.port = clientPort;
		//dbglog( "[master] reconnectMessage.header.token %08x\n", reconnectMessage.header.token );
		//dbglog( "[master] reconnectMessage.header.length %d\n", reconnectMessage.header.length );
		//dbglog( "[master] reconnectMessage.header.type %x\n", reconnectMessage.header.type );
		//dbglog( "[master] reconnectMessage.port %d\n", reconnectMessage.port );
		//dbglog( "[master] Send the connecting client the new port %d.\n", reconnectMessage.port );
		int bytesSent = 0;
		if( ( bytesSent = sendMessage( SocketBase, clientSocket, (ProtocolMessage_t*)&reconnectMessage ) ) != reconnectMessage.header.length )
		{
			dbglog( "[master] Only sent %d bytes of %d.\n", bytesSent, reconnectMessage.header.length );
		}		

		//Generate a new client entry
		ClientThread_t *clientThread = AllocVec( sizeof( *clientThread ), MEMF_CLEAR|MEMF_FAST );
		clientThread->process = clientProcess;
		clientThread->port = clientPort;
		clientThread->messagePort = clientMessage->mn_ReplyPort;

		//Get the peer name
		struct sockaddr_in name;
		socklen_t nameLen = sizeof( name );
		getpeername( clientSocket, (struct sockaddr *)&name, &nameLen );
		memcpy( clientThread->ip, &name.sin_addr.s_addr, 4 );
		dbglog( "[Master] created thread for IP %u.%u.%u.%u:%d\n", 
									(UBYTE)clientThread->ip[ 0 ],
									(UBYTE)clientThread->ip[ 1 ],
									(UBYTE)clientThread->ip[ 2 ],
									(UBYTE)clientThread->ip[ 3 ],
									clientThread->port );

		addClientThreadToList( clientThread );
		dbglog( "[master] Done handling the new inbound connection.\n" );

		CloseSocket( clientSocket );
	}
}


//Entry point for the spawned process.  Wraps the real body so that, no matter
//which of the body's many return paths is taken, we always decrement the live
//thread counter exactly once as our final act before returning into the system.
static void clientThread()
{
	clientThreadBody();
	Forbid(); g_ActiveThreadCount--; Permit();
}

static void clientThreadBody()
{
	dbglog( "[child] Client thread started.\n" );

	struct Library *SocketBase = NULL;
	struct sockaddr_in addr __attribute__((aligned(4))) ;
	LONG returnCode = 0;
	unsigned short newPort = g_NextClientPort++;		//Here we will also store our client port

	//saftey check for the port number
	if( g_NextClientPort > 50000 ) g_NextClientPort = 40000;

	//Find the master server port
	dbglog( "[child] Searching for parent process port.\n" );
	Forbid();
	struct MsgPort *masterPort = FindPort( MASTER_MSGPORT_NAME );
	Permit();
	if( masterPort == NULL )
	{
		dbglog( "[child] Couldn't find master port.  Aborting child process.\n" );
		return;
	}
	//dbglog( "[child] MsgPort = 0x%08x\n", (unsigned int)masterPort );


	//Create a reply message port
	struct MsgPort *replyPort = CreateMsgPort();
	if( replyPort == (struct MsgPort *)NULL )
	{
		dbglog( "[child] Failed to create a reply message port.\n" );
		return;
	}

	//Ready our new client message to send to the parent process
	struct AEMessage newClientMessage __attribute__((aligned(4)));
	memset( &newClientMessage, 0, sizeof( newClientMessage ) );
	newClientMessage.msg.mn_Node.ln_Type = NT_MESSAGE;
	newClientMessage.msg.mn_Length = sizeof( struct AEMessage );
	newClientMessage.msg.mn_ReplyPort = replyPort;
	newClientMessage.messageType = AEM_KillClient;
	newClientMessage.port = newPort;

	//Open the BSD Socket library
	dbglog( "[child] Opening bsdsocket.library.\n" );

	//Did we open the bsdsocket library successfully?
	SocketBase = OpenLibrary("bsdsocket.library", 4 );
	if( !SocketBase )
	{
		dbglog( "[child] Failed to open the bsdsocket.library.\n" );
		PutMsg( masterPort, (struct Message *)&newClientMessage );
		WaitPort( replyPort );
		DeleteMsgPort( replyPort );
		lockClientThreadList();
		removeClientByPort( newPort );
		unlockClientThreadList();
		dbglog( "[child] Exiting.\n" );
		if( SocketBase != NULL ) { CloseLibrary( SocketBase ); SocketBase = NULL; }
		return;
	}

	//Open a new server port for this client
	dbglog( "[child]  Opening client socket.\n" );
	SOCKET childServerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if( childServerSocket == SOCKET_ERROR)
	{
		dbglog( "[child] Error opening client socket.\n" );
		PutMsg( masterPort, (struct Message *)&newClientMessage );
		WaitPort( replyPort );
		DeleteMsgPort( replyPort );
		lockClientThreadList();
		removeClientByPort( newPort );
		unlockClientThreadList();
		dbglog( "[child] Exiting.\n" );
		if( SocketBase != NULL ) { CloseLibrary( SocketBase ); SocketBase = NULL; }
		return;
	}

	dbglog( "[child] Setting socket options.\n" );
	int yes = 1;
	//int no = 0;
	returnCode = setsockopt( childServerSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	if(returnCode == SOCKET_ERROR)
	{
		dbglog( "[child] Error setting socket options.\n" );
		PutMsg( masterPort, (struct Message *)&newClientMessage );
		WaitPort( replyPort );
		dbglog( "[child] Exiting.\n" );
		DeleteMsgPort( replyPort );
		lockClientThreadList();
		removeClientByPort( newPort );
		unlockClientThreadList();
		CloseSocket( childServerSocket );
		if( SocketBase != NULL ) { CloseLibrary( SocketBase ); SocketBase = NULL; }
		return;
	}

	//setting the bind port
	dbglog( "[child] Binding to port %d\n", newPort );
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons( newPort );
	returnCode = bind( childServerSocket, (struct sockaddr *)&addr, sizeof(addr));
	if(returnCode == SOCKET_ERROR)
	{
		dbglog( "[child] Unable to bind to port %d.\n", newPort );
		PutMsg( masterPort, (struct Message *)&newClientMessage );
		WaitPort( replyPort );
		dbglog( "[child] Exiting.\n" );
		DeleteMsgPort( replyPort );
		lockClientThreadList();
		removeClientByPort( newPort );
		unlockClientThreadList();
		CloseSocket( childServerSocket );
		if( SocketBase != NULL ) { CloseLibrary( SocketBase ); SocketBase = NULL; }
		return;
	}

	dbglog( "[child] Enable port listening on port %d.\n", newPort );
	returnCode = listen( childServerSocket, 1 );
	if(returnCode == SOCKET_ERROR)
	{
		dbglog( "[child] Unable to bind to port %d.\n", newPort );
		PutMsg( masterPort, (struct Message *)&newClientMessage );
		WaitPort( replyPort );
		dbglog( "[child] Exiting.\n" );
		DeleteMsgPort( replyPort );
		lockClientThreadList();
		removeClientByPort( newPort );
		unlockClientThreadList();
		CloseSocket( childServerSocket );
		if( SocketBase != NULL ) { CloseLibrary( SocketBase ); SocketBase = NULL; }
		return;
	}

	//Send the socket to the parent process
	dbglog( "[child] Informing the parent which port the client should reconnect on.\n" );
	newClientMessage.messageType = AEM_NewClient;	//New client message
	//dbglog( "[child] Sending message ( message: 0x%08x )\n", (unsigned int)&newClientMessage );
	PutMsg( masterPort, (struct Message *)&newClientMessage );
	WaitPort( replyPort );


	//Get the socket we will be using
	dbglog( "[child] Waiting for confirmation from parent.\n" );
	struct SocketHandleMessage *portMessage = (struct SocketHandleMessage *)WaitPort( replyPort );
	if( portMessage == NULL )
	{
		dbglog( "[child] Got a null back from parent.  Do we care though?\n" );
	}
	//dbglog( "[child] Got reply from parent.  Message address: 0x%08x\n", (unsigned int)portMessage );

	//Now wait for the client to connect
	dbglog("[child] Awaiting new connection\n" );

	socklen_t addrLen __attribute__((aligned(4))) = sizeof( addr );

	//Make the listen socket non-blocking so we can honour a server shutdown while
	//waiting for the client to reconnect, instead of blocking forever in accept().
	int nbFlag = 1;
	IoctlSocket( childServerSocket, FIONBIO, &nbFlag );

	SOCKET newClientSocket = -1;
	LONG acceptTimeout = 500;	//~10s at Delay(2); ample for a reconnect
	while( g_KeepServerRunning && acceptTimeout-- > 0 )
	{
		newClientSocket = (SOCKET)accept( childServerSocket, (struct sockaddr *)&addr, &addrLen );
		if( newClientSocket >= 0 ) break;
		Delay( 2 );
	}
	if( newClientSocket < 0 )
	{
		dbglog( "[child] No client reconnect (server shutdown or timeout).  Aborting.\n" );
		goto exit_child;
	}

	//Put the accepted socket back into blocking mode, as the rest of the code expects.
	int blkFlag = 0;
	IoctlSocket( newClientSocket, FIONBIO, &blkFlag );

	//Start the listen loop
	dbglog( "[child] Accepting client connection from handle %d.\n", newClientSocket );


	//Get current buffer sizes
	ULONG sendBufferSize = 0;
	ULONG receiveBufferSize = 0;
	socklen_t varLen = sizeof( sendBufferSize );

	returnCode = getsockopt( newClientSocket, SOL_SOCKET, SO_RCVBUF, &receiveBufferSize, &varLen );
	returnCode = getsockopt( newClientSocket, SOL_SOCKET, SO_SNDBUF, &sendBufferSize, &varLen );
	dbglog( "[child] Buffer sizes %lu (snd) %lu (rcv)\n", sendBufferSize, receiveBufferSize );

	//The previous stall workaround shrank these to 1514*3 (~4.5KB), which caps the TCP
	//window (and therefore throughput ~= window/RTT) far below what a 100Mbit link can
	//sustain.  The actual stall cause was Nagle/delayed-ACK interaction (see TCP_NODELAY
	//below), not buffer size, so we can give the socket a generous window instead.
	sendBufferSize = 65536;
	receiveBufferSize = 65536;
	returnCode = setsockopt( newClientSocket, SOL_SOCKET, SO_RCVBUF, &receiveBufferSize, varLen );
	returnCode = setsockopt( newClientSocket, SOL_SOCKET, SO_SNDBUF, &sendBufferSize, varLen );

	//Check that they took hold
	returnCode = getsockopt( newClientSocket, SOL_SOCKET, SO_RCVBUF, &receiveBufferSize, &varLen );
	returnCode = getsockopt( newClientSocket, SOL_SOCKET, SO_SNDBUF, &sendBufferSize, &varLen );
	dbglog( "[child] Adjusted buffer sizes %lu (snd) %lu (rcv)\n", sendBufferSize, receiveBufferSize );

	//Disable Nagle's algorithm.  Without this, small writes (headers, acks, chunk
	//confirms) sit buffered waiting to coalesce and interact badly with the peer's
	//delayed ACKs, producing the multi-10ms "stalls" the buffer shrink above was
	//originally (mis-)diagnosed to fix.
	static int noDelayYes = 1;
	returnCode = setsockopt( newClientSocket, IPPROTO_TCP, TCP_NODELAY, &noDelayYes, sizeof( noDelayYes ) );
	dbglog( "[child] TCP_NODELAY setsockopt returned %ld\n", returnCode );

	//Enable the keep alive so that crashed clients won't keep the client thread alive
	static int keepAliveYes = 1;
	returnCode = setsockopt( newClientSocket, SOL_SOCKET, SO_KEEPALIVE, &keepAliveYes, sizeof( keepAliveYes ) );


	//Let's reserve some memory for each of the messages
	ProtocolMessage_t *message __attribute__((aligned(4))) = AllocVec( MAX_MESSAGE_LENGTH, MEMF_FAST|MEMF_CLEAR );

	//Create a filesend context
	FileSendContext_t *fileSendContext = allocateFileSendContext();

	//Start reading all inbound messages
	int bytesRead = 0;
	volatile char keepThisConnectionRunning = 1;
	LONG bytesAvailable = 0;
	LONG inactivityCount = 300;
	while( keepThisConnectionRunning && g_KeepServerRunning )
	{

		//Check if there is any
		IoctlSocket( newClientSocket,FIONREAD ,&bytesAvailable );
		if( bytesAvailable == 0 )
		{
			//Do we have any messages on our message port?
			struct Message *newMessage = GetMsg( replyPort );
			if( newMessage != NULL )
			{
				//dbglog( "[child] We got a message on the message port.\n" );
				//If this is a terminate message, start shutting down the clients
				struct AEMessage *aeMsg = (struct AEMessage *)newMessage;
				if( aeMsg->messageType == AEM_KillClient )
				{
					dbglog( "[child] Received kill client.\n" );

					//Form the disconnect message
					ProtocolMessageDisconnect_t disconnectMessage;
					disconnectMessage.header.length = sizeof( disconnectMessage );
					disconnectMessage.header.type = PMT_CLOSING;
					disconnectMessage.header.token = MAGIC_TOKEN;
					snprintf( disconnectMessage.message, sizeof( disconnectMessage.message ), "Shutting down server.  Sorry, but you are out." );

					//Send the disconnect
					sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&disconnectMessage );

					//Give the message time to be deliverey by the stack
					dbglog( "[child] Sent disconnect message.  Delaying to allow delivery.\n" );
					//Delay( 5 );

					//Send a reply back to the caller
					dbglog( "[child] Acknowledging the master's request.\n" );
					ReplyMsg( newMessage );

					keepThisConnectionRunning = 0;
					dbglog( "[child] Staring the shutdown.\n" );
					goto exit_child;
				}
			}

			//Check if we have reached the inactivity limit
			if( inactivityCount-- == 0 )
			{
				dbglog( "[child] Pinging client due to inactivity.\n" );
				static ProtocolMessage_t ping = { MAGIC_TOKEN, PMT_PING, sizeof( ProtocolMessage_t ) };
				int returnCode = sendMessage( SocketBase, newClientSocket, &ping );
				if( returnCode == SOCKET_ERROR )
				{
					//This connection is gone
					keepThisConnectionRunning = 0;
					dbglog( "[child] Closing connection due to timeout.\n" );
				}else
				{
					inactivityCount = 300;
				}
			}


			Delay( 5 );
			continue;
		}

		//Reset the inactivity counter
		inactivityCount = 300;

		//Pull in the next message from the socket
		//dbglog( "[child] Reading next network message.\n" );
		bytesRead = getMessage( SocketBase, newClientSocket, message, MAX_MESSAGE_LENGTH );
		//dbglog( "[child] Message read containing %d bytes.\n", bytesRead );

		//sanity check
		if( bytesRead < 0 )
		{
			//Form the disconnect message
			ProtocolMessageDisconnect_t disconnectMessage;
			disconnectMessage.header.length = sizeof( disconnectMessage );
			disconnectMessage.header.type = PMT_CLOSING;
			disconnectMessage.header.token = MAGIC_TOKEN;

			switch( bytesRead )
			{
				case MAGIC_TOKEN_MISSING:
					dbglog( "[child] Magic Token missing in message.  Terminating Connection.\n" );
					//Send the disconnect
					snprintf( disconnectMessage.message, sizeof( disconnectMessage.message ), "Magic token in packet was wrong or missing.  Disconnecting." );
					sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&disconnectMessage );
					keepThisConnectionRunning = 0;
					continue;
				break;
				case INVALID_MESSAGE_TYPE:
					dbglog( "[child] Received message of an invalid type.  Terminating Connection.\n" );
					//Send the disconnect
					snprintf( disconnectMessage.message, sizeof( disconnectMessage.message ), "Server received an invalid message type.  Disconnecting." );
					sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&disconnectMessage );
					keepThisConnectionRunning = 0;
					continue;
				break;
				case INVALID_MESSAGE_SIZE:
					dbglog( "[child] Received a message of an invalid size.  Terminating Connection.\n" );
					//Send the disconnect
					snprintf( disconnectMessage.message, sizeof( disconnectMessage.message ), "Message size was wrong.  Disconnecting." );
					sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&disconnectMessage );
					keepThisConnectionRunning = 0;
					continue;
				break;
				case SOCKET_ERROR:
					dbglog( "[child] Socket error on client connection.shutting down.\n" );
					keepThisConnectionRunning = 0;
					continue;
				break;
			}
		}

		if( bytesRead < sizeof( ProtocolMessage_t ))
		{
			Delay( 5 );
			continue;	//This can't be a valid message then
		}

		//Handle the message
		switch( message->type )
		{
			case PMT_SHUTDOWN_SERVER:
				dbglog( "[child] Shutting down the server.\n" );
				g_KeepServerRunning = 0;
			break;
			case PMT_GET_VERSION:
				dbglog( "[child] Server Version Requested.\n" );
				ProtocolMessage_Version_t versionMessage __attribute__((aligned(4))) =
				{
						.header.token = MAGIC_TOKEN,
						.header.length = sizeof( ProtocolMessage_Version_t ),
						.header.type = PMT_VERSION,
						.major = VERSION_MAJOR,
						.minor = VERSION_MINOR,
						.rev = VERSION_REVISION,
						.releaseType = RELEASE_TYPE
				};
				dbglog( "[child] Sending Version back\n" );
				sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&versionMessage );
				dbglog( "[child] Version sent.\n" );
			break;
			case PMT_GET_DIR_LIST:
			{
				//Extract the desired path
				ProtocolMessageGetDirectoryList_t *getDir = (ProtocolMessageGetDirectoryList_t*)message;
				char dirPath[ MAX_FILEPATH_LENGTH ] __attribute__((aligned(4)));
				memset( dirPath, 0, sizeof( dirPath ) );
				memcpy( dirPath, getDir->path, getDir->length );

				//Sanity check
				if( strlen( dirPath ) == 0 || dirPath[ MAX_FILEPATH_LENGTH - 1 ] != 0 )
				{
					dbglog( "[child] Ignoring invalid path in GET_DIR_LIST request.\n" );
					break;;
				}

				dbglog( "[child] Directory list for '%s' (%d bytes long) Requested.\n", getDir->path, getDir->length );

				//Get the requested list
				ProtocolMessageDirectoryList_t *list = getDirectoryList( dirPath );
				if( list == NULL )
				{
					dbglog( "[child] Unable to get directory listing '%s'.\n", getDir->path );
					char *errorMessage = "That path doesn't exist.";
					unsigned int messageLength = sizeof( ProtocolMessage_Failed_t ) + strlen( errorMessage );
					ProtocolMessage_Failed_t *failedMessage = AllocVec( messageLength, MEMF_CLEAR|MEMF_FAST );
					failedMessage->header.length = messageLength;
					failedMessage->header.token = MAGIC_TOKEN;
					failedMessage->header.type = PMT_FAILED;
					CopyMem( errorMessage, failedMessage->message, strlen( errorMessage ) );
					sendMessage( SocketBase, newClientSocket,(ProtocolMessage_t*)failedMessage );
					FreeVec( failedMessage );
					break;
				}
				sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)list );
				break;
			}
			case PMT_GET_FILE:
			{
				ProtocolMessage_FilePull_t *getFileMsg = ( ProtocolMessage_FilePull_t* )message;
				unsigned int numberOfChunks __attribute__((aligned(4))) = 0;
				(void)numberOfChunks;
				int bytesSent __attribute__((aligned(4))) = 0;

				//Get the file path from the message
				char filePath[ MAX_FILEPATH_LENGTH ] __attribute__((aligned(4)));
				memset( filePath, 0, sizeof( filePath ) );
				strncpy( filePath, getFileMsg->filePath, sizeof( filePath ) );

				//Sanity check
				if( strlen( filePath ) == 0 || filePath[ MAX_FILEPATH_LENGTH - 1 ] != 0 )
				{
					dbglog( "[child] Ignoring invalid file path in GET_FILE request.\n" );
					break;
				}


				dbglog( "[child] Get file called for file '%s'\n", filePath );

				//Can this file be sent?
				ProtocolMessage_Ack_t *acknowldgeMessage = requestFileSend( filePath, fileSendContext );
				if( acknowldgeMessage == NULL )	break;
				dbglog( "[child] sending acknowledge message address: 0x%08x\n", (unsigned int)acknowldgeMessage );
				char response = acknowldgeMessage->response;
				dbglog( "[child] sending acknowledge with response: %d\n", response );
				bytesSent = sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)acknowldgeMessage );
				if( response == 0  || bytesSent < 0 )
				{
					dbglog( "[child] File simply cannot be sent (response: %d).\n", response );
					break;		//This file can't be opened.  Nothing more to do
				}

				//Send the start of the file transfer
				dbglog( "[child] Sending start of file\n" );
				ProtocolMessage_StartOfFileSend_t *startOfSendfileMessage = getStartOfFileSend( filePath, fileSendContext );
				if( startOfSendfileMessage == 0 )	break;
#if 0
				dbglog( "[child] StartOfFile Message address: 0x%08x\n", (unsigned int)startOfSendfileMessage );
				dbglog( "[child] StartOfFile Message token: 0x%08x\n", startOfSendfileMessage->header.token );
				dbglog( "[child] StartOfFile Message type: 0x%08x\n", startOfSendfileMessage->header.type );
				dbglog( "[child] StartOfFile Message length: 0x%08x\n", startOfSendfileMessage->header.length );
#endif
				numberOfChunks = startOfSendfileMessage->numberOfFileChunks;
				bytesSent = sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)startOfSendfileMessage );

				//If the file can't be sent for whatever reason, the filesize will be zero
				if( startOfSendfileMessage->fileSize == 0 || bytesSent < 0 )
				{
					dbglog( "[child] Somehow, the file can nolonger be sent.  Cleaning up and stopping.\n" );
					dbglog( "[child] Bytes sent: %d.\n", bytesSent );
					dbglog( "[child] File Size: %d.\n", startOfSendfileMessage->fileSize );
					cleanupFileSend( fileSendContext );
					break;		//This file can't be sent for some reason.
				}

				//If we have made it this far, we must be able to send the file.
				//Let's keep asking for file chunks until there are no more to send
				ProtocolMessage_FileChunk_t *nextFileChunk = NULL;
				dbglog( "[child] Starting file chunk sending\n" );
				LONG bytesAvailable __attribute__((aligned(4))) = 0;
				while( ( nextFileChunk = getNextFileSendChunk( filePath, fileSendContext ) ) )
				{
					//Abort immediately if the server is shutting down, so we never
					//keep the master waiting through a whole file transfer.
					if( !g_KeepServerRunning )
					{
						dbglog( "[child] Server shutting down, aborting file send.\n" );
						cleanupFileSend( fileSendContext );
						keepThisConnectionRunning = FALSE;
						break;
					}

					bytesAvailable = 0;
					dbglog( "[child] Sending the next chunk %d of %d (size %db/%db)\n", nextFileChunk->chunkNumber, numberOfChunks, nextFileChunk->bytesContained, nextFileChunk->header.length );
					bytesSent = sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)nextFileChunk );
					if( bytesSent < 0 )
					{
						dbglog( "[child] error sending file chunk (error %d).  Abondoning connection.\n", bytesSent )
						{
							cleanupFileReceive();
							keepThisConnectionRunning = FALSE;
							break;
						}
					}

					//Check occassionally for  an abort request
					if( nextFileChunk->chunkNumber%10 == 0 )
					{
						dbglog( "[child] Checking for cancel message......" );
						IoctlSocket( newClientSocket,FIONREAD ,&bytesAvailable );
						if( bytesAvailable )
						{
							bytesRead = getMessage( SocketBase, newClientSocket, message, MAX_MESSAGE_LENGTH );
							if( message->type == PMT_CANCEL_OPERATION )
							{
								dbglog( "found!\n");
								dbglog( "[child] Got a request to abort the file download.  Cleaning up and stopping.\n" );
								cleanupFileSend( fileSendContext );
								break;
							}
						}
						else
						{
							dbglog( "not found!\n" );
						}
					}
				}

				//Clean up
				dbglog( "[child] Cleaning up after sending file.\n" );
				cleanupFileSend( fileSendContext );
				Delay( 2 );

				break;
			}
			case PMT_PUT_FILE:
			{

				ProtocolMessage_Ack_t ackMessage __attribute__((aligned(4)));
				ackMessage.header.length = sizeof( ackMessage );
				ackMessage.header.token = MAGIC_TOKEN;
				ackMessage.header.type = PMT_ACK;
				ackMessage.response = 1;
				

				ProtocolMessage_FileChunkConfirm_t fileChunkConfMessage;
				fileChunkConfMessage.header.length = sizeof( fileChunkConfMessage );
				fileChunkConfMessage.header.token = MAGIC_TOKEN;
				fileChunkConfMessage.header.type = PMT_FILE_CHUNK_CONF;
				fileChunkConfMessage.chunkNumber = 0;

				ProtocolMessage_FilePutConfirm_t filePutConfirm __attribute__((aligned(4)));
				filePutConfirm.header.length = sizeof( filePutConfirm );
				filePutConfirm.header.token = MAGIC_TOKEN;
				filePutConfirm.header.type = PMT_FILE_PUT_CONF;

				dbglog( "[child] Got a PUT_FILE request.\n" );
				//First we expect the FilePut request
				ProtocolMessage_FilePut_t *filePutMessage = ( ProtocolMessage_FilePut_t* )message;
				char filePath[ MAX_FILEPATH_LENGTH ] __attribute__((aligned(4)));
				strncpy( filePath, filePutMessage->filePath, MAX_FILEPATH_LENGTH );

				//We need to check that we can write the requested file
				ProtocolMessage_Ack_t *acknowledgeMessage = requestFileReceive( filePath );
				int bytesSent = sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)acknowledgeMessage );
				(void)bytesSent;
				//dbglog( "[child] %d bytes sent during sending of ACK.\n", bytesSent );

				//Is it possible to write this file?
				//If not, we need to abort here
				if( acknowledgeMessage->response != AT_OK )
				{
					dbglog( "[child] Unable to write file '%s'.  Aborting.\n", filePath );
					break;
				}
				dbglog( "[child] It seems file '%s' can be uploaded.\n", filePath );

				//Now we expect the startoffilesend message
				dbglog( "[child] Now just waiting for the start-of-filesend message.\n" );
				IoctlSocket( newClientSocket,FIONREAD ,&bytesAvailable );
				int retries = 50;
				while( ( bytesAvailable < sizeof( ProtocolMessage_t ) ) && ( retries > 0 ) )
				{
					dbglog( "[child] Waiting for start-of-file-send message (retries remaining %d)\n", retries );
					retries--;
					Delay( 2 );
					IoctlSocket( newClientSocket,FIONREAD ,&bytesAvailable );
				}

				if( getMessage( SocketBase, newClientSocket, message, MAX_MESSAGE_LENGTH ) < sizeof( ProtocolMessage_StartOfFileSend_t ) ||
						message->type != PMT_START_OF_SEND_FILE )
				{
					dbglog( "[child] Expected a Start-of-Filesend message.  But that didn't arrive.  Aborting.\n" );
					dbglog( "[child] Token 0x%08x Type 0x%08x Length %04u\n", message->token, message->type, message->length );
					sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&ackMessage );
					cleanupFileReceive();
					break;
				}
				
				ProtocolMessage_StartOfFileSend_t *startOfFileSend = ( ProtocolMessage_StartOfFileSend_t* )message;
				dbglog( "[child] PutStartOfFileReceive address: 0x%08x ChunkCount %d Filesize %d\n", (unsigned int)startOfFileSend, startOfFileSend->numberOfFileChunks, startOfFileSend->fileSize );
				putStartOfFileReceive( startOfFileSend );

				//Special Case.  If the file is zero bytes
				if( startOfFileSend->numberOfFileChunks == 0 )
				{
					dbglog( "[child] Empty file sent.\n" );
					cleanupFileReceive();
					break;
				}

				//Acknowledge that we have the SOF
				dbglog( "[child] acknowledging start-of-file receive.\n" );
				sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&ackMessage );


				//Now start pulling the file chunks
				unsigned int chunksRemaining = startOfFileSend->numberOfFileChunks;
				dbglog( "[child] Now just waiting for the file chunks to arrive.\n" );
				do
				{
					//Abort immediately if the server is shutting down.
					if( !g_KeepServerRunning )
					{
						dbglog( "[child] Server shutting down, aborting file receive.\n" );
						cleanupFileReceive();
						keepThisConnectionRunning = 0;
						goto exit_child;
					}

					//Let's give the client some time to send the next chunk, else we make a timeout.
					//This blocks on WaitSelect() until data actually arrives (or the timeout
					//expires) instead of polling FIONREAD with a Delay(2) (40ms) between checks.
					//That polling granularity taxed every single chunk boundary - with a 5 chunk
					//(160KB) in-flight window, 160KB/40ms is ~4MB/s, which was exactly the
					//throughput ceiling being seen - independent of any TCP-level tuning.
					fd_set waitFdSet;
					FD_ZERO( &waitFdSet );
					FD_SET( newClientSocket, &waitFdSet );
					struct timeval selectTimeout;
					selectTimeout.tv_sec = 2;	//2s stall timeout, matches the previous 50*Delay(2) budget
					selectTimeout.tv_usec = 0;
					int waitResult = WaitSelect( newClientSocket + 1, &waitFdSet, NULL, NULL, &selectTimeout, NULL );

					//Did we time out waiting for the client?
					if( waitResult <= 0 )
					{
						//So a timeout occurred.  Close the connection and cleanup
						dbglog( "[child] There was a timeout on recieving file chunks from the client.  Terminating the conection.\n" );
						ackMessage.response = 0;
						sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&ackMessage );
						cleanupFileReceive();
						keepThisConnectionRunning = 0;
						goto exit_child;
					}

					//Get the next file chunk message
					int bytesReceived = getMessage( SocketBase, newClientSocket, message, MAX_MESSAGE_LENGTH );
					//dbglog( "[child] Got %d bytes waiting for the file chunk.\n", bytesReceived );

					//Is this a cancel message?
					if( message->type == PMT_CANCEL_OPERATION )
					{
						dbglog( "[child] Received a cancel notification from the client.  Aborting.\n" );
						cleanupFileReceive();
						break;
					}

					//Invalid message?
					if(  bytesReceived < message->length || message->type != 0x00000011 )
					{
						dbglog( "[child] Received an invalid message awaiting a file chunk.  Aborting.\n" );
						filePutConfirm.filesize = getBytesWritenToFile();
						sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&filePutConfirm );	//Send back what we got until now
						cleanupFileReceive();
						break;
					}

					ProtocolMessage_FileChunk_t *fileChunkMessage = ( ProtocolMessage_FileChunk_t* )message;
					chunksRemaining = putNextFileSendChunk( fileChunkMessage );
					fileChunkConfMessage.chunkNumber = fileChunkMessage->chunkNumber;
					sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&fileChunkConfMessage );	//Send an ack back
					dbglog( "[child] Got file chunk %d.  Chunks remaining: %d\r", fileChunkMessage->chunkNumber, chunksRemaining );

				}while( chunksRemaining );

				//Send the confirmation and cleanup
				unsigned int bytesWritten = getBytesWritenToFile();
				filePutConfirm.filesize = bytesWritten;
				sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&filePutConfirm );	//Send back what we got until now
				dbglog( "\n[child] File receive completed (%u bytes).\n", bytesWritten );
				

				//We are done
				cleanupFileReceive();
				Delay( 2 );
				break;
			}
			case PMT_DELETE_PATH:
			{
				ProtocolMessage_DeletePath_t *deleteMessage = (ProtocolMessage_DeletePath_t*)message;
				dbglog( "[child] Deleting path: %s\n", deleteMessage->filePath );

				//If we are just deleting a file
				if( deleteMessage->entryType == DET_FILE )
				{
					ProtocolMessage_PathDeleted_t *msg = deleteFile( deleteMessage->filePath );
					sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)msg );
					//dbglog( "[child] Delete of %s was %s", deleteMessage->filePath, msg->deleteSucceeded == 0 ? "unsuccessful" : "successful" );
				}
				else if( deleteMessage->entryType == DET_USERDIR )
				{
					ULONG deleteRetryCount = 30;
					BOOL deleteFailures = FALSE;

					if( !startRecursiveDelete( deleteMessage->filePath ) )
					{
						dbglog( "[child] Recursive delete started.\n" );
						ProtocolMessage_PathDeleted_t *msg = NULL;
						while( ( msg = getNextFileDeleted() ) )
						{
							//dbglog( "[child] Deleted %s %s\r", msg->filePath, msg->deleteSucceeded == 0 ? "unsuccessfully" : "successfully" );
							if( msg->deleteSucceeded == 0 )	deleteFailures = TRUE;
							sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)msg );
						}
						endRecursiveDelete();

						//If there were some failures try again
						while( deleteFailures == TRUE && deleteRetryCount-- > 0 )
						{
							Delay( 10 );
							deleteFailures = FALSE;
							if( !startRecursiveDelete( deleteMessage->filePath ) )
							{
								dbglog( "[child] Had some failures so we are trying again.\n" );
								ProtocolMessage_PathDeleted_t *msg = NULL;
								while( ( msg = getNextFileDeleted() ) )
								{
									//dbglog( "[child] Deleted %s %s\r", msg->filePath, msg->deleteSucceeded == 0 ? "unsuccessfully" : "successfully" );
									if( msg->deleteSucceeded == 0 )	deleteFailures = TRUE;
									sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)msg );
								}
								endRecursiveDelete();
							}else
							{
								endRecursiveDelete();
								break;
							}
						}

						//Now send completion message
						msg = getRecusiveDeleteCompletedMessage();
						sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)msg );
						dbglog( "[child] Recursive delete completed.\n" );
					}else
					{
						endRecursiveDelete();
						dbglog( "[child] Failed to start recursive delete of %s\n", deleteMessage->filePath );
						ProtocolMessage_PathDeleted_t *msg = getDeleteError();
						sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)msg );
					}

				}else
				{
					dbglog( "[child] Unknown file type.  Can't delete this.\n" );
				}

				break;
			}
			case PMT_RENAME_FILE:
			{
				dbglog( "[child] Rename of a file requested.\n" );
				ProtocolMessage_RenamePath_t *renameMessage = ( ProtocolMessage_RenamePath_t *)message;
				char *oldName = renameMessage->filePaths;
				char *newName = renameMessage->filePaths + renameMessage->oldNameSize + 1;
				
				//Perform the rename
				Rename( (STRPTR)oldName, (STRPTR)newName );
				dbglog( "[child] Renamed '%s' to '%s'.\n", oldName, newName );
			}
			case PMT_MKDIR:
			{
				dbglog( "[child] Received request to make directory.\n" );
				ProtocolMessage_MakeDir_t *mkdirMessage = ( ProtocolMessage_MakeDir_t* )message;
				char dirPath[ MAX_FILEPATH_LENGTH ];
				strncpy( dirPath, mkdirMessage->filePath, MAX_FILEPATH_LENGTH );
				dbglog( "[child] Requested to create dir: %s\n", dirPath );

				//Prepare the response
				ProtocolMessage_Ack_t ackMessage = {
						.header.token = MAGIC_TOKEN,
						.header.type = PMT_ACK,
						.header.length = sizeof( ProtocolMessage_Ack_t )
				};

				//Add the result of the operation
				char returnCode = makeDir( dirPath );
				ackMessage.response = returnCode;
				if( returnCode > 0 )
				{
					dbglog( "[child] Failed to create dir: %s\n", dirPath );
				}
				else
				{
					dbglog( "[child] Succeeded in creating dir: %s\n", dirPath );
				}

				//Send the ack
				sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&ackMessage );

				break;
			}
			case PMT_GET_VOLUMES:
			{
				dbglog( "[child] Got a request to retrieve the list of volumes.\n" );
				ProtocolMessage_VolumeList_t *volumeListMessage = getVolumeList();
				if( volumeListMessage == NULL )
				{
					dbglog( "[child] Unknown error in retreiving the list of volumes.\n" );
					break;
				}

				//Send the list
				sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)volumeListMessage );
				break;
			}
			case PMT_RUN:
			{
				dbglog( "[child] Received request to run.  Getting the command.\n" );
				ProtocolMessage_Run_t *runMessage = ( ProtocolMessage_Run_t* )message;
				char command[ 128 ];
				strncpy( command, runMessage->command, 128 );
				dbglog( "[child] Received request to run '%s'.\n", command );
				dbglog( "[child] Got return code %d from the command.\n", (int)returnCode );
				break;
			}
			case PMT_REBOOT:
			{

				dbglog( "[child] Reboot requested.\n" );
#if 1
				//Delay( 500 );
				ColdReboot();
#else
				Execute( "Reboot", 0, 0 );
#endif
				break;
			}
			default:
				dbglog( "[child] Unsupported message type 0x%08x\n", message->type );
			break;
		}
	}

exit_child: ;

	//If we are leaving because the whole server is shutting down, let the PC
	//client know politely before we drop the socket.
	if( !g_KeepServerRunning )
	{
		ProtocolMessageDisconnect_t disconnectMessage;
		disconnectMessage.header.length = sizeof( disconnectMessage );
		disconnectMessage.header.type = PMT_CLOSING;
		disconnectMessage.header.token = MAGIC_TOKEN;
		snprintf( disconnectMessage.message, sizeof( disconnectMessage.message ), "Server is shutting down." );
		sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&disconnectMessage );
	}

	//Free the file send context
	freeFileSendContext( fileSendContext );

	//Remove this client from the list
	dbglog( "[client] Locking the client list.\n" );
	lockClientThreadList();
	dbglog( "[client] Removing ourselves from the client list.\n" );
	removeClientByPort( newPort );
	dbglog( "[client] Unlocking the client list.\n" );
	unlockClientThreadList();

	//Close our message port.  We need to clear it out first though.
	dbglog( "[child] Deleting message port.\n" );
	DeleteMsgPort( replyPort );

	//Free our messagebuffer
	dbglog( "[child] Freeing message buffer.\n" );
	if( message ) FreeVec( message );

	//Now close the sockets because we are done here
	if( newClientSocket >= 0 )
	{
		dbglog( "[child] Closing client connection socket.\n" );
		CloseSocket( newClientSocket );
	}
	dbglog( "[child] Closing client thread for socket 0x%08x.\n", childServerSocket );
	CloseSocket( childServerSocket );

	//Close bsdsocket.library from this task.  Each client thread opens its OWN
	//bsdsocket base; if it is never closed, the library's open count never returns
	//to baseline and the TCP/IP stack cannot be shut down (which blocks WHDLoad).
	if( SocketBase != NULL )
	{
		dbglog( "[child] Closing bsdsocket.library.\n" );
		CloseLibrary( SocketBase );
		SocketBase = NULL;
	}

	dbglog( "[child] Terminating.\n" );
	return;
}

