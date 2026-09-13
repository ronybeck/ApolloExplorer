#define DBGOUT 0

#include <sys/types.h>
#include <dos/dos.h>            /* SIGBREAKF_CTRL_C */
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>
#include <clib/wb_protos.h>
#include <sys/ioctl.h>

#include "protocolTypes.h"
#include "protocol.h"
#include "AETypes.h"
#include "AEUtil.h"
#include "AEClientThread.h"
#include "AEDiscoveryThread.h"
#include "wbicon.h"

UBYTE *ver = "\0$VER: ApolloExplorerSrv " VERSION_STRING;

struct Library *SocketBase = NULL;

/*****************************************************************************/

#include <proto/exec.h>
#define __BSDSOCKET_NOLIBBASE__
#include <proto/bsdsocket.h>

char g_KeepServerRunning = 1;

/* Number of spawned child processes (client threads + discovery thread) that are
 * currently alive.  The master must not return from main() — which lets the CLI
 * unload our seglist — until this reaches 0, otherwise children still executing
 * code in that seglist crash/hang.  Incremented by the master when a child is
 * spawned, decremented by the child itself as its very last act. */
volatile LONG g_ActiveThreadCount = 0;

char* g_MessagePortName __attribute((aligned(4))) = MASTER_MSGPORT_NAME;
struct MsgPort *g_AEServerMessagePort = NULL;
char g_ShowWBIcon = 1;
char g_UseArgsName = 0;
char g_ArgsName[32] = { 0 };

/*****************************************************************************/

/*
 * Wait for ONE specific message to come back on 'replyPort', for at most
 * 'timeoutTicks' (1 tick = 1/50 s).  Returns TRUE if the expected ack arrived,
 * FALSE on timeout.  'replyPort' MUST be a private port dedicated to the
 * handshake so no unrelated command can be mistaken for the ack.
 */
static BOOL waitForAck( struct MsgPort *replyPort, struct Message *expected, ULONG timeoutTicks )
{
	while( timeoutTicks > 0 )
	{
		struct Message *m = GetMsg( replyPort );
		if( m != NULL )
		{
			if( m == expected )
				return TRUE;					//Our ack: done
			if( m->mn_ReplyPort != NULL && m->mn_Node.ln_Type == NT_MESSAGE )
				ReplyMsg( m );					//Someone else's: don't strand them
			continue;
		}
		Delay( 1 );							//20 ms
		timeoutTicks--;
	}
	return FALSE;							//Timed out
}

/*****************************************************************************/

#define ARGS_TEMPLATE "NAME/K,NOICON/s"
BOOL readArguments( )
{
	LONG result[2] = { 0, 0 };
	struct RDArgs *rdargs;
	
	//Get the string supplied at execution
	STRPTR cmdLineArgs = GetArgStr();
	if( cmdLineArgs == NULL )
	{
		dbglog( "No command line arguments found.\n" );
		return FALSE;
	}

	//Parse the command line arguments
	if (rdargs = (struct RDArgs *)ReadArgs(ARGS_TEMPLATE, result, NULL))
	{
		//Get the arguments we need
		if( result[0] )
		{
			dbglog( "Read Name [%s] from command line arguments.\n", (char*)result[ 0 ]);
			g_UseArgsName = 1;
			strcpy( g_ArgsName, (char*)result[ 0 ] );
		}
		if( (BOOL)result[1] )
		{
			dbglog( "Disabling workbench icon.\n");
			g_ShowWBIcon = 0;
		}
		return TRUE;
	}
	else
	{
		printf( "Failed to read command line arguments.\n" );
		return FALSE;
	}

	FreeArgs(rdargs);

	return TRUE;
}

int main(int argc, char *argv[])
{
	dbglog( "starting ApolloExplorer Version: %s\n", VERSION_STRING );

	struct sockaddr_in addr __attribute__((aligned(4)));
	//SOCKET s = 0;
	SOCKET serverSocket __attribute__((aligned(4))) = -1;
	int res = 0;

	//We want to do at least a few attempts to start this
	//Maybe the TCP/Stack is starting in the back ground
	//and we need to give it a few seconds to start
	int numberOfRestartAttempts = 15;

	//Read the arguments passed at the CMD
	readArguments();

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

	//Setup the desktop icon
	//First the text
	char iconText[] = "Stop ApolloExplorer";


	UWORD iconWidth = 41;
	UWORD iconHeight = 45;
	UWORD *wbIconImageData = NULL;
	struct Image wbIconImage;
	struct DiskObject iconDiskObject;
	struct AppIcon *appIcon = NULL;


	if( g_ShowWBIcon )
	{
		//Copy the icon object into chipram
		dbglog( "[master] Copying the workbench image data\n" );
		wbIconImageData = AllocVec( sizeof( iconData ), MEMF_CHIP|MEMF_CLEAR );
		memcpy( wbIconImageData, &iconData, sizeof( iconData ) );

		//Setup the image struct
		dbglog( "[master] Setup the image struct for the workbench icon\n" );
		wbIconImage.Width = iconWidth;
		wbIconImage.Height = iconHeight;
		wbIconImage.Depth = 2;
		wbIconImage.PlaneOnOff = 0x0000;
		wbIconImage.PlanePick = 0x0003;
		wbIconImage.NextImage = NULL;
		wbIconImage.ImageData = wbIconImageData;

		//Now setup the disk object
		dbglog( "[master] Setup the DiskObject for the workbench icon\n" );
		memset( &iconDiskObject, 0, sizeof( iconDiskObject ) );
		iconDiskObject.do_Gadget.Width = iconWidth;
		iconDiskObject.do_Gadget.Height = iconHeight;
		iconDiskObject.do_Gadget.GadgetRender = &wbIconImage;
		iconDiskObject.do_CurrentX = NO_ICON_POSITION;
		iconDiskObject.do_CurrentY = NO_ICON_POSITION;

		//Add the app icon to the workbench
		dbglog( "[master] Adding the app icon to workbench.\n" );
		appIcon = AddAppIcon(	0,
								0,
								iconText,
								g_AEServerMessagePort,
								0,
								&iconDiskObject,
								TAG_END );
	}

	//Start announcing our presence on the network
	startDiscoveryThread();

	//Start listening for new connections
	while( g_KeepServerRunning )
	{
		//We don't want 100% CPU usage
		Delay( 5 );

		//Last-resort exit: allow CTRL-C / Break <task> to bring the server down
		//cleanly even if no client/Tool is able to talk to us.
		if( CheckSignal( SIGBREAKF_CTRL_C ) )
		{
			dbglog( "[master] CTRL-C received, shutting down.\n" );
			g_KeepServerRunning = 0;
			break;
		}

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
			if( aeMsg->messageType == AEM_Shutdown || aeMsg->messageType == WB_ICON_DOUBLE_CLICKED )
			{
				dbglog( "[master] Got a shutdown message.\n" );

				//remove the icon immediately
				if( appIcon != NULL ) RemoveAppIcon( appIcon );
				appIcon = NULL;

				//Signal every cooperating task (discovery thread + all client threads)
				//to stop.  They each observe g_KeepServerRunning and unwind on their own.
				//The orderly, time-bounded teardown happens at the 'shutdown:' label below.
				g_KeepServerRunning = 0;

				//Acknowledge the caller (e.g. ApolloExplorerTool) IMMEDIATELY so it never
				//blocks waiting on us, then fall through to the bounded teardown.
				dbglog( "[master] Acknowledging the caller's shutdown request.\n" );
				newMessage->mn_Node.ln_Type = NT_REPLYMSG;
				ReplyMsg( newMessage );

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
				LONG messageSize = sizeof( struct AEClientList ) * clientCount + sizeof( struct AEClientList ); 	//Bigger than necessary but keeps the maths simple.
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
				ClientThread_t *clientListCopy = getClientThreadList();
				ClientThread_t *node = clientListCopy->next;
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

				//Free the temporary copy returned by getClientThreadList().
				freeClientThreadList( clientListCopy );

				PutMsg( newMessage->mn_ReplyPort, (struct Message*)clientListMessage );
			}else{
				dbglog( "[master] We got an unknown message (0x%08x) on the message port.\n", (unsigned int)aeMsg->messageType );
			}
		}
	}

	shutdown:
	dbglog( "[master] Shutting down.\n" );

	//Make sure every cooperating task knows we are leaving.
	g_KeepServerRunning = 0;

	//Remove the app icon
	if( appIcon != NULL ) RemoveAppIcon( appIcon );
	appIcon = NULL;

	//Stop the discovery thread.  Bounded handshake on a PRIVATE reply port so
	//that nothing other than the ack can ever satisfy the wait, and a timeout
	//so we never hang if it has already gone.
	{
		struct MsgPort *shutdownReplyPort = CreateMsgPort();
		Forbid();
		struct MsgPort *discoveryMsgPort = FindPort( DISCOVERY_MESSAGE_PORT_NAME );
		Permit();
		if( discoveryMsgPort != NULL && shutdownReplyPort != NULL )
		{
			dbglog( "[master] Signalling the discovery thread to terminate.\n" );
			struct AEMessage killDiscoveryMessage;
			memset( &killDiscoveryMessage, 0, sizeof( killDiscoveryMessage ) );
			killDiscoveryMessage.messageType = AEM_Shutdown;
			killDiscoveryMessage.msg.mn_Length = sizeof( killDiscoveryMessage );
			killDiscoveryMessage.msg.mn_ReplyPort = shutdownReplyPort;
			killDiscoveryMessage.msg.mn_Node.ln_Type = NT_MESSAGE;
			PutMsg( discoveryMsgPort, (struct Message*)&killDiscoveryMessage );
			if( !waitForAck( shutdownReplyPort, (struct Message*)&killDiscoveryMessage, 150 ) )
			{
				dbglog( "[master] Discovery did not ack within 3s; continuing.\n" );
			}
			else
			{
				dbglog( "[master] Discovery acknowledged.\n" );
			}
		}
		if( shutdownReplyPort != NULL ) DeleteMsgPort( shutdownReplyPort );
	}

	//Wait (time-bounded) for EVERY spawned child process (client threads AND the
	//discovery thread) to fully exit before we return from main().  Returning
	//while a child is still executing lets the CLI unload our seglist out from
	//under it, which crashes/hangs that child into an unkillable task.  The
	//counter only hits 0 once each child has run its final cleanup.
	{
		ULONG drainTicks = 500;	//up to ~10s
		for( ;; )
		{
			LONG remaining = g_ActiveThreadCount;
			if( remaining <= 0 )
			{
				dbglog( "[master] All child threads have ended.\n" );
			}
			if( remaining <= 0 ) break;
			if( drainTicks == 0 )
			{
				dbglog( "[master] Timeout waiting for %ld child thread(s); forcing shutdown.\n", remaining );
				break;
			}
			Delay( 2 );
			drainTicks -= 2;
		}
	}

	//Give the children's final return-into-system a moment to complete before the
	//seglist can be unloaded.
	Delay( 5 );

	if( wbIconImageData != NULL ) FreeVec( wbIconImageData );

	//Free the client list sentinels (all client entries are gone by now).
	destroyClientThreadList();

	//Clear out and then free the message port
	dbglog( "[master] Removing message port.\n" );
	if( g_AEServerMessagePort != NULL )
	{
		RemPort( g_AEServerMessagePort );
		DeleteMsgPort( g_AEServerMessagePort );
		g_AEServerMessagePort = NULL;
	}

	//Shutdown the socket.
	dbglog( "[master] Closing socket.\n" );
	if( SocketBase != NULL && serverSocket >= 0 ) CloseSocket( serverSocket );

	//Close bsdsocket.library from the task that opened it.  Leaving it open can
	//stall this task's exit on some TCP stacks.
	if( SocketBase != NULL )
	{
		CloseLibrary( SocketBase );
		SocketBase = NULL;
	}

	dbglog( "[master] Terminating.\n" );
	return 0;
}




