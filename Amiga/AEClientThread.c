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


extern char g_KeepServerRunning;

static unsigned short g_NextClientPort = MAIN_LISTEN_PORTNUMBER + 1;

static void clientThread();

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
		//dbglog( "[master] Child started.\n" );

		//Wait for the child to request the socket handle
		dbglog( "[master] Waiting for child tell us what port the client should reconnect on.\n" );
		struct AEMessage *newClientMessage = (struct AEMessage *)WaitPort( msgPort );
		struct Message *clientMessage = GetMsg( msgPort );	//Remove this message from the queue
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


static void clientThread()
{
	dbglog( "[child] Client thread started.\n" );

	struct Library *SocketBase = NULL;
	struct sockaddr_in addr;
	int returnCode = 0;
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
	struct AEMessage newClientMessage;
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

	socklen_t addrLen = sizeof( addr );
	SOCKET newClientSocket = (SOCKET)accept( childServerSocket, (struct sockaddr *)&addr, &addrLen);
	if( newClientSocket < 0 )
	{
		dbglog( "[child] accept error for socket %d - Error number %d \"%s\"\n", newClientSocket, errno, strerror( errno ) );
		goto exit_child;
	}

	//Start the listen loop
	dbglog( "[child] Accepting client connection from handle %d.\n", newClientSocket );

	//Let's reserve some memory for each of the messages
	ProtocolMessage_t *message = AllocVec( MAX_MESSAGE_LENGTH, MEMF_FAST|MEMF_CLEAR );

	//Start reading all inbound messages
	int bytesRead = 0;
	volatile char keepThisConnectionRunning = 1;
	LONG bytesAvailable = 0;
	LONG driveRefreshCounter = 2000;
	while( keepThisConnectionRunning )
	{
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
					Delay( 5 );

					//Send a reply back to the caller
					dbglog( "[child] Acknowledging the master's request.\n" );
					ReplyMsg( newMessage );

					keepThisConnectionRunning = 0;
					dbglog( "[child] Staring the shutdown.\n" );
					goto exit_child;
				}
			}

			//Is it time to refresh the drive list yet?
			if( driveRefreshCounter-- == 0 )
			{
				dbglog( "[child] Refreshing the list of volumes.\n" );
				ProtocolMessage_VolumeList_t *volumeListMessage = getVolumeList();
				if( volumeListMessage == NULL )
				{
					dbglog( "[child] Unknown error in retreiving the list of volumes.\n" );
					return;
				}

				//Send the list
				sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)volumeListMessage );
				FreeVec( volumeListMessage );

				//Reset the count
				driveRefreshCounter = 1000;
			}

			Delay( 5 );
			continue;
		}

		//Reset the count
		driveRefreshCounter = 1000;

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
			continue;	//This can't be a valid message then

		//Handle the message
		switch( message->type )
		{
			case PMT_SHUTDOWN_SERVER:
				dbglog( "[child] Shutting down the server.\n" );
				g_KeepServerRunning = 0;
			break;
			case PMT_GET_VERSION:
				dbglog( "[child] Server Version Requested.\n" );
				ProtocolMessage_Version_t versionMessage =
				{
						.header.token = MAGIC_TOKEN,
						.header.length = sizeof( ProtocolMessage_Version_t ),
						.header.type = PMT_VERSION,
						.major = VERSION_MAJOR,
						.minor = VERSION_MINOR,
						.rev = VERSION_REVISION
				};
				dbglog( "[child] Sending Version back\n" );
				sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&versionMessage );
				dbglog( "[child] Version sent.\n" );
			break;
			case PMT_GET_DIR_LIST:
			{
				//Extract the desired path
				ProtocolMessageGetDirectoryList_t *getDir = (ProtocolMessageGetDirectoryList_t*)message;
				char dirPath[ MAX_FILEPATH_LENGTH ];
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
					//TODO: perhaps it would be better to send feedback here to the client.
					break;
				}
				sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)list );
				FreeVec( list );
				break;
			}
			case PMT_GET_FILE:
			{
				ProtocolMessage_FilePull_t *getFileMsg = ( ProtocolMessage_FilePull_t* )message;
				unsigned int numberOfChunks = 0;
				(void)numberOfChunks;
				int bytesSent = 0;

				//Get the file path from the message
				char filePath[ MAX_FILEPATH_LENGTH ];
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
				ProtocolMessage_Ack_t *acknowldgeMessage = requestFileSend( filePath );
				dbglog( "[child] sending acknowledge message address: 0x%08x\n", (unsigned int)acknowldgeMessage );
				char response = acknowldgeMessage->response;
				dbglog( "[child] sending acknowledge with response: %d\n", response );
				bytesSent = sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)acknowldgeMessage );
				if( response == 0  || bytesSent < 0 )
				{
					dbglog( "[child] File simply cannot be sent.\n" );
					break;		//This file can't be opened.  Nothing more to do
				}

				//Send the start of the file transfer
				dbglog( "[child] Sending start of file\n" );
				ProtocolMessage_StartOfFileSend_t *startOfSendfileMessage = getStartOfFileSend( filePath );
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
					cleanupFileSend();
					break;		//This file can't be sent for some reason.
				}

				//If we have made it this far, we must be able to send the file.
				//Let's keep asking for file chunks until there are no more to send
				ProtocolMessage_FileChunk_t *nextFileChunk = NULL;
				dbglog( "[child] Starting file chunk sending\n" );
				LONG bytesAvailable = 0;
				while( ( nextFileChunk = getNextFileSendChunk( filePath ) ) )
				{
					bytesAvailable = 0;
					dbglog( "[child] Sending the next chunk %d of %d.\n", nextFileChunk->chunkNumber, numberOfChunks );
					sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)nextFileChunk );

					//Check for an incoming message (e.g. an abort request)
					IoctlSocket( newClientSocket,FIONREAD ,&bytesAvailable );
					if( bytesAvailable )
					{
						bytesRead = getMessage( SocketBase, newClientSocket, message, MAX_MESSAGE_LENGTH );
						if( message->type == PMT_CANCEL_OPERATION )
						{
							dbglog( "[child] SGot a request to abort the file download.  Cleaning up and stopping.\n" );
							cleanupFileSend();
							break;
						}
					}
				}

				//Clean up
				dbglog( "[child] Cleaning up after sending file.\n" );
				cleanupFileSend();
				Delay( 2 );

				break;
			}
			case PMT_PUT_FILE:
			{
				dbglog( "[child] Got a PUT_FILE request.\n" );
				//First we expect the FilePut request
				ProtocolMessage_FilePut_t *filePutMessage = ( ProtocolMessage_FilePut_t* )message;
				char filePath[ MAX_FILEPATH_LENGTH ];
				strncpy( filePath, filePutMessage->filePath, MAX_FILEPATH_LENGTH );

				//We need to check that we can write the requested file
				ProtocolMessage_Ack_t *acknowledgeMessage = requestFileReceive( filePath );
				int bytesSent = sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)acknowledgeMessage );
				(void)bytesSent;
				//dbglog( "[child] %d bytes sent during sending of ACK.\n", bytesSent );

				//Is it possible to write this file?
				//If not, we need to abort here
				if( acknowledgeMessage->response == 0 )
				{
					dbglog( "[child] Unable to write file '%s'.  Aborting.\n", filePath );
					break;
				}
				dbglog( "[child] It seems file '%s' can be uploaded.\n", filePath );

				//Now we expect the startoffilesend message
				dbglog( "[child] Now just waiting for the start-of-filesend message.\n" );
				if( getMessage( SocketBase, newClientSocket, message, MAX_MESSAGE_LENGTH ) < sizeof( ProtocolMessage_StartOfFileSend_t ) ||
						message->type != PMT_START_OF_SEND_FILE )
				{
					dbglog( "[child] Expected a Start-of-Filesend message.  But that didn't arrive.  Aborting.\n" );
					cleanupFileReceive();
					break;
				}
				dbglog( "[child] Got the start-of-filesend message.\n" );
				ProtocolMessage_StartOfFileSend_t *startOfFileSend = ( ProtocolMessage_StartOfFileSend_t* )message;
				dbglog( "[child] PutStartOfFileReceive address: 0x%08x.\n", (unsigned int)startOfFileSend );
				dbglog( "[child] PutStartOfFileReceive number of chunks: %d.\n", startOfFileSend->numberOfFileChunks );
				dbglog( "[child] PutStartOfFileReceive number of filesize: %d.\n", startOfFileSend->fileSize );
				putStartOfFileReceive( startOfFileSend );

				//Special Case.  If the file is zero bytes
				if( startOfFileSend->numberOfFileChunks == 0 )
				{
					dbglog( "[child] Empty file sent.\n" );
					cleanupFileReceive();
					break;
				}

				ProtocolMessage_Ack_t ackMessage;
				ackMessage.header.length = sizeof( ackMessage );
				ackMessage.header.token = MAGIC_TOKEN;
				ackMessage.header.type = PMT_ACK;
				ackMessage.response = 1;


				//Now start pulling the file chunks
				unsigned int chunksRemaining = startOfFileSend->numberOfFileChunks;
				dbglog( "[child] Now just waiting for the file chunks to arrive.\n" );
				do
				{
					//Let's give the client some time to send the next chunk, else we make a timeout
					LONG bytesAvailable = 0;
					LONG retryCount = 10;
					while( bytesAvailable == 0 && retryCount-- > 0 )
					{
						IoctlSocket( newClientSocket, FIONREAD ,&bytesAvailable );
						if( bytesAvailable != 0 )
						{
							break;
						}
						dbglog( "[child] Waiting on client to send bytes...... (waiting %ld)\n", retryCount );
						Delay( 20 );
					}

					//Are there some bytes here now?
					if( bytesAvailable == 0 )
					{
						//So a timeout occurred.  Close the connection and cleanup
						dbglog( "[child] There was a timeout on recieving file chunks from the client.  Terminating the conection.\n" );
						cleanupFileReceive();
						keepThisConnectionRunning = 0;
						goto exit_child;
					}

					//Get the next file chunk message
					int bytesReceived = getMessage( SocketBase, newClientSocket, message, MAX_MESSAGE_LENGTH );
					dbglog( "[child] Got %d bytes waiting for the file chunk.\n", bytesReceived );

					//Is this a cancel message?
					if( message->type == PMT_CANCEL_OPERATION )
					{
						dbglog( "[child] Received a cancel notification from the client.  Aborting.\n" );
						cleanupFileReceive();
						break;
					}

					//Invalid message?
					if(  bytesReceived < sizeof( ProtocolMessage_FileChunk_t ) || message->type != 0x00000011 )
					{
						dbglog( "[child] Received an invalid message awaiting a file chunk.  Aborting.\n" );
						cleanupFileReceive();
						break;
					}

					ProtocolMessage_FileChunk_t *fileChunkMessage = ( ProtocolMessage_FileChunk_t* )message;
					chunksRemaining = putNextFileSendChunk( fileChunkMessage );
					//sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&ackMessage );	//Send an ack back
					dbglog( "[child] Got file chunk %d.  Chunks remaining: %d\n", fileChunkMessage->chunkNumber, chunksRemaining );

				}while( chunksRemaining );
				cleanupFileReceive();

				//We are done
				dbglog( "[child] File receive completed.\n" );
				Delay( 2 );
				break;
			}
			case PMT_DELETE_PATH:
			{
				driveRefreshCounter = 1000;
				ProtocolMessage_DeletePath_t *deleteMessage = (ProtocolMessage_DeletePath_t*)message;
				char dirPath[ MAX_FILEPATH_LENGTH * 2 ] = "";
				memset( dirPath, 0, sizeof( dirPath ) );
				strncpy( dirPath, deleteMessage->filePath, sizeof( dirPath ) );
				dbglog( "[child] Requested to delete path: %s\n", dirPath );

				//Sanity check
				if( strlen( dirPath ) == 0 || dirPath[ MAX_FILEPATH_LENGTH - 1 ] != 0 )
				{
					//Probably this is a corrupt file name
					dbglog( "[child] Requested to delete path which looks to be corrupt.  Ignoring\n" );

					//Let's inform the client
					char *errorString = "The file name supplied by the client looks corrupt.";
					ULONG stringSize = strlen( errorString );

					//Create an error message
					ProtocolMessage_Failed_t *errorMessage = AllocVec( stringSize, MEMF_FAST|MEMF_CLEAR );

					if( errorMessage == NULL )
					{
						//Well this is embarrassing.  No memory allocated.
						dbglog( "[child] unable to allocate error message for failed delete operation.\n" );
						break;
					}

					errorMessage->header.token = MAGIC_TOKEN;
					errorMessage->header.length = sizeof( *errorMessage ) + stringSize;
					errorMessage->header.type = PMT_FAILED;
					snprintf( errorMessage->message, stringSize + 1, "%s", errorString );

					//send the message back to the server
					sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)errorMessage );

					//Free the message
					FreeVec( errorMessage );
					break;
				}

				//Peform the delete
				if( !DeleteFile( dirPath ) )
				{
					dbglog( "[child] Requested to delete path: %s.  But this was not possible.\n", dirPath );

					//Form an error message and send it to the client
					char *errorStringStart = "Unable to delete path: ";
					ULONG stringSize = strlen( errorStringStart ) + strlen( dirPath ) ;
					ProtocolMessage_Failed_t *errorMessage = AllocVec( stringSize, MEMF_FAST|MEMF_CLEAR );

					if( errorMessage == NULL )
					{
						//Well this is embarrassing.  No memory allocated.
						dbglog( "[child] unable to allocate error message for failed delete operation.\n" );
						break;
					}

					errorMessage->header.token = MAGIC_TOKEN;
					errorMessage->header.length = sizeof( *errorMessage ) + stringSize;
					errorMessage->header.type = PMT_FAILED;
					snprintf( errorMessage->message, stringSize + 1, "%s %s", errorStringStart, dirPath );

					//send the message back to the server
					sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)errorMessage );

					//Free the message
					FreeVec( errorMessage );
				}else
				{
					dbglog( "[child] Checking if  %s is deleted.\n", dirPath );
					//Test that the file is gone
					BPTR fileLock = 1;
					int retries = 30;
					while( fileLock != 0 && retries--!=0 )
					{
						Delay( 10 );		//Wait a bit.  Perhaps it is done after this wait
						fileLock = Lock( dirPath, ACCESS_READ );
						if( fileLock != 0 )
						{
							UnLock( fileLock );
						}
					}

					//Check that the file was really deleted
					if( fileLock != 0 )
					{
						dbglog( "[child] Path %s is NOT deleted.\n", dirPath );

						//Somehow the delete failed or timedout
						char *errorString = "Deletion timed-out.";
						ULONG stringSize = strlen( errorString );

						//Create an error message
						ProtocolMessage_Failed_t *errorMessage = AllocVec( stringSize, MEMF_FAST|MEMF_CLEAR );

						if( errorMessage == NULL )
						{
							//Well this is embarrassing.  No memory allocated.
							dbglog( "[child] unable to allocate error message for failed delete operation.\n" );
							break;
						}

						errorMessage->header.token = MAGIC_TOKEN;
						errorMessage->header.length = sizeof( *errorMessage ) + stringSize;
						errorMessage->header.type = PMT_FAILED;
						snprintf( errorMessage->message, stringSize + 1, "%s", errorString );

						//send the message back to the server
						sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)errorMessage );

						//Free the message
						FreeVec( errorMessage );
						break;

					}

					dbglog( "[child] Path %s is deleted.\n", dirPath );

					//Send an ACK
					ProtocolMessage_Ack_t ackMessage;
					ackMessage.header.token = MAGIC_TOKEN;
					ackMessage.header.length = sizeof( ackMessage );
					ackMessage.header.type = PMT_ACK;
					ackMessage.response = 1;

					//send the message back to the server
					sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&ackMessage );
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
				if( !makeDir( dirPath ) )
				{
					dbglog( "[child] Failed to create dir: %s\n", dirPath );
					ackMessage.response = 0;
				}
				else
				{
					dbglog( "[child] Succeeded in creating dir: %s\n", dirPath );
					ackMessage.response = 1;
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
					return;
				}

				//Send the list
				sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)volumeListMessage );
				FreeVec( volumeListMessage );
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

	//Remove this client from the list
	dbglog( "[client] Locking the client list.\n" );
	lockClientThreadList();
	dbglog( "[client] Removing ourselves from the client list.\n" );
	removeClientByPort( newPort );
	dbglog( "[client] Unlocking the client list.\n" );
	unlockClientThreadList();

	//Close our message port.  We need to clear it out first though.
	dbglog( "[child] Deleting message port.\n" );
	// struct Message *newMessage = GetMsg( replyPort );
	// while( newMessage != NULL )
	// {
	// 	ReplyMsg( newMessage );
	// 	newMessage = GetMsg( replyPort );
	// }
	DeleteMsgPort( replyPort );

	//Free our messagebuffer
	dbglog( "[child] Freeing message buffer.\n" );
	FreeVec( message );

	//Now close the socket because we are done here
	dbglog( "[child] Closing client thread for socket 0x%08x.\n", childServerSocket );
	CloseSocket( childServerSocket );

	dbglog( "[child] Terminating.\n" );
	return;
}

