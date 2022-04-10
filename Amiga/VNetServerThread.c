/*
 * VNetServerThread.c
 *
 *  Created on: May 16, 2021
 *      Author: rony
 */

#define DBGOUT 1

#ifdef __GNUC__
#include <stdio.h>
#include <unistd.h>
#include <sys/filio.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dostags.h>
#define __BSDSOCKET_NOLIBBASE__
#include <proto/bsdsocket.h>
#endif

#ifdef __VBCC__

#endif

#include "VNetServerThread.h"
#include "VNetUtil.h"
#include "DirectoryList.h"
#include "SendFile.h"
#include "ReceiveFile.h"
#include "MakeDir.h"
#include "VolumeList.h"


extern char g_KeepServerRunning;

static unsigned short g_NextClientPort = MAIN_LISTEN_PORTNUMBER + 1;

struct NewClientMessage
{
	struct Message msg;
	unsigned short port;
	char killClient;		//Set this if the client should be rejected
};

static void clientThread();

void startClientThread( struct Library *SocketBase, struct MsgPort *msgPort, SOCKET clientSocket )
{
	dbglog( "[master] Accepted socket connection.\n" );
	if ( DOSBase )
	{
#if 0
		BPTR consoleHandle = Open( "CONSOLE:", MODE_OLDFILE );
		if( consoleHandle )
		{
			dbglog( "[master] Console handle opened.\n" );
		}
		else
		{
			dbglog( "[master] Console handle NOT opened.\n" );
		}
#endif

		//Start a client thread
		struct TagItem tags[] = {
				{ NP_StackSize,		16384 },
				{ NP_Name,			(ULONG)"VNetClientThread" },
				{ NP_Entry,			(ULONG)clientThread },
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
		dbglog( "[master] Child started.\n" );

		//Wait for the child to request the socket handle
		dbglog( "[master] Waiting for child tell us what port the client should reconnect on.\n" );
		struct NewClientMessage *newClientMessage = (struct NewClientMessage *)WaitPort( msgPort );
		GetMsg( msgPort );	//Remove this message from the queue
		unsigned short clientPort = newClientMessage->port;

		//Did the child request to kill the client?
		if( newClientMessage->killClient )
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
		dbglog( "[master] reconnectMessage.header.token %08x\n", reconnectMessage.header.token );
		dbglog( "[master] reconnectMessage.header.length %d\n", reconnectMessage.header.length );
		dbglog( "[master] reconnectMessage.header.type %x\n", reconnectMessage.header.type );
		dbglog( "[master] reconnectMessage.port %d\n", reconnectMessage.port );
		dbglog( "[master] Send the connecting client the new port %d.\n", reconnectMessage.port );
		int bytesSent = 0;
		if( ( bytesSent = sendMessage( SocketBase, clientSocket, (ProtocolMessage_t*)&reconnectMessage ) ) != reconnectMessage.header.length )
		{
			dbglog( "[master] Only sent %d bytes of %d.\n", bytesSent, reconnectMessage.header.length );
		}
		dbglog( "[master] Done handling the new inbound connection.\n" );

		CloseSocket( clientSocket );
	}
}


static void clientThread()
{
	dbglog( "[child] Client thread started.\n" );

	struct Library *DOSBase = OpenLibrary( "dos.library", 0 );
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
	dbglog( "[child] MsgPort = 0x%08x\n", (unsigned int)masterPort );


	//Create a reply message port
	struct MsgPort *replyPort = CreateMsgPort();
	if( replyPort == (struct MsgPort *)NULL )
	{
		dbglog( "[child] Failed to create a reply message port.\n" );
		return;
	}

	//Ready our new client message to send to the parent process
	struct NewClientMessage newClientMessage = {
													.msg.mn_Node.ln_Type = NT_MESSAGE,
													.msg.mn_Length = sizeof( struct NewClientMessage ),
													.msg.mn_ReplyPort = replyPort,
													.killClient = 1,		//default incase we have to terminate the client
													.port = newPort
												};

	//Open the BSD Socket library
	dbglog( "[child] Opening bsdsocket.library.\n" );

	//Did we open the bsdsocket library successfully?
	SocketBase = OpenLibrary("bsdsocket.library", 4 );
	if( !SocketBase )
	{
		dbglog( "[child] Failed to open the bsdsocket.library.\n" );
		PutMsg( masterPort, (struct Message *)&newClientMessage );
		WaitPort( replyPort );
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
		dbglog( "[child] Exiting.\n" );
		return;
	}

	dbglog( "[child] Setting socket options.\n" );
	int yes = 1;
	returnCode = setsockopt( childServerSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	if(returnCode == SOCKET_ERROR)
	{
		dbglog( "[child] Error setting socket options.\n" );
		PutMsg( masterPort, (struct Message *)&newClientMessage );
		WaitPort( replyPort );
		dbglog( "[child] Exiting.\n" );
		DeleteMsgPort( replyPort );
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
		CloseSocket( childServerSocket );
		return;
	}

	//Send the socket to the parent process
	dbglog( "[child] Informing the parent which port the client should reconnect on.\n" );
	newClientMessage.killClient = 0;	//Disable the 'kill' command first though
	dbglog( "[child] Sending message ( message: 0x%08x )\n", (unsigned int)&newClientMessage );
	PutMsg( masterPort, (struct Message *)&newClientMessage );


	//Get the socket we will be using
	dbglog( "[child] Waiting for confirmation from parent.\n" );
	struct SocketHandleMessage *portMessage = (struct SocketHandleMessage *)WaitPort( replyPort );
	if( portMessage == NULL )
	{
		dbglog( "[child] Got a null back from parent.  Do we care though?\n" );
		return;
	}
	dbglog( "[child] Got reply from parent.  Message address: 0x%08x\n", (unsigned int)portMessage );
	DeleteMsgPort( replyPort );

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
	char keepThisConnectionRunning = 1;
	while( g_KeepServerRunning && keepThisConnectionRunning )
	{
		//Pull in the next message from the socket
		dbglog( "[child] Waiting for the next message.\n" );
		bytesRead = getMessage( SocketBase, newClientSocket, message, MAX_MESSAGE_LENGTH );
		//dbglog( "[child] Message read containing %d bytes.\n", bytesRead );

		//sanity check
		if( bytesRead < 0 )
		{
			switch( bytesRead )
			{
				case MAGIC_TOKEN_MISSING:
					dbglog( "[child] Magic Token missing in message.  Terminating Connection.\n" );
					keepThisConnectionRunning = 0;
					continue;
				break;
				case INVALID_MESSAGE_TYPE:
					dbglog( "[child] Received message of an invalid type.  Terminating Connection.\n" );
					keepThisConnectionRunning = 0;
					continue;
				break;
				case INVALID_MESSAGE_SIZE:
					dbglog( "[child] Received a message of an invalid size.  Terminating Connection.\n" );
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
				sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&versionMessage );
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

					//Check for an incomine message (e.g. an abort request)
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


				//Now start pulling the file chunks
				unsigned int chunksRemaining = startOfFileSend->numberOfFileChunks;
				dbglog( "[child] Now just waiting for the file chunks to arrive.\n" );
				do
				{
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
					dbglog( "[child] Got file chunk %d.  Chunks remaining: %d\n", fileChunkMessage->chunkNumber, chunksRemaining );

				}while( chunksRemaining );

				//We are done
				dbglog( "[child] File receive completed.\n" );
				Delay( 2 );
				break;
			}
			case PMT_DELETE_PATH:
			{
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
					//Test that the file is gone
					BPTR fileLock = 1;
					int retries = 10;
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

					//Send an ACK
					ProtocolMessage_Ack_t message;
					message.header.token = MAGIC_TOKEN;
					message.header.length = sizeof( message );
					message.header.type = PMT_ACK;
					message.response = 1;

					//send the message back to the server
					sendMessage( SocketBase, newClientSocket, (ProtocolMessage_t*)&message );
				}

				break;
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
				break;
			}
			case PMT_RUN:
			{
				dbglog( "[child] Received request to run.  Getting the command.\n" );
				ProtocolMessage_Run_t *runMessage = ( ProtocolMessage_Run_t* )message;
				char command[ 128 ];
				strncpy( command, runMessage->command, 128 );
				dbglog( "[child] Received request to run '%s'.\n", command );
				LONG returnCode;
				(void)returnCode;
#if 0
				returnCode = Execute( command , newClientSocket, NULL );
#endif

#if 1
				/*
				BPTR consoleHandle = Open( "CON:0/40/640/150/Auto", MODE_OLDFILE );
				if( !consoleHandle )
				{
					dbglog( "[child] Failed to open the console.\n" );
					return;
				}
				*/

				returnCode = SystemTags(
							(STRPTR)command,
							SYS_Input, NULL,
							SYS_Output, newClientSocket,
							TAG_DONE );
#endif

#if 0
				dbglog( "[child] Opening console.\n" );
				BPTR consoleHandle = Open( "CON:0/40/640/150/Auto", MODE_OLDFILE );
				if( !consoleHandle )
				{
					dbglog( "[child] Failed to open the console.\n" );
					return;
				}

				//Setup some selects
				dbglog( "[child] Setting up the select variables.\n" );
				fd_set networkReadSet;
				fd_set consoleReadSet;
				char buffer;
				struct timeval timeOut = { .tv_sec = 0, .tv_usec = 100000 };	//100ms
				FD_ZERO( &networkReadSet );
				FD_ZERO( &consoleReadSet );
				FD_SET( newClientSocket, &networkReadSet );
				FD_SET( consoleHandle, &consoleReadSet );


				//Now loop feeding the network socket to CON: and vica versa
				dbglog( "[child] Start the polling loop.\n" );
				while( 1 )
				{
					dbglog( "[child] Looping.\n" );
					FD_ZERO( &networkReadSet );
					FD_ZERO( &consoleReadSet );

					//Wait on the network first
					WaitSelect( newClientSocket+1,&networkReadSet, NULL, NULL, &timeOut, NULL );

					//Anything to send?
					if( FD_ISSET( newClientSocket, &networkReadSet ) )
					{
						recv( newClientSocket, &buffer, 1, 0 );
						dbglog( "Got from network: %c", buffer );
						Write( consoleHandle, &buffer, 1 );
					}

					//Wait on the console now
					WaitSelect( consoleHandle+1,&consoleReadSet, NULL, NULL, &timeOut, NULL );

					//Anything to send?
					if( FD_ISSET( consoleHandle, &consoleReadSet ) )
					{
						recv( consoleHandle, &buffer, 1, 0 );
						dbglog( "Got from console: %c", buffer );
						send( newClientSocket, &buffer, 1, 0 );
					}
				}

				FD_CLR( newClientSocket, &networkReadSet );
				FD_CLR( consoleHandle, &consoleReadSet );
				Close( consoleHandle );
				Close( newClientSocket );
#endif

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

	exit_child:
	//Free our messagebuffer
	FreeVec( message );

	//Now close the socket because we are done here
	dbglog( "[child] Closing client thread for socket 0x%08x.\n", childServerSocket );
	CloseSocket( childServerSocket );

	CloseLibrary( DOSBase );
}

