/*
 * AEDiscoveryThread.c
 *
 *  Created on: Jul 10, 2021
 *      Author: rony
 */


#include <stdio.h>
#include <unistd.h>
#include <sys/filio.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dostags.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <exec/lists.h>
#include <sys/types.h>
#include <clib/icon_protos.h>
#include <clib/dos_protos.h>
#define __BSDSOCKET_NOLIBBASE__
#include <proto/bsdsocket.h>

#define DBGOUT 1

#include "protocol.h"
#include "AEUtil.h"
#include "AETypes.h"
#include "AEDiscoveryThread.h"

#define VREG_BOARD_Unknown      0x00      // Unknown
#define VREG_BOARD_V600         0x01      // V600
#define VREG_BOARD_V500         0x02      // V500
#define VREG_BOARD_V4           0x03      // V4-500
#define VREG_BOARD_ICEDRAKE     0x04      // V4-Icedrake
#define VREG_BOARD_V4SA         0x05      // V4SA
#define VREG_BOARD_V1200        0x06      // V1200
#define VREG_BOARD_V4_V600      0x07      // V4-V600 = MantiCore
#define VREG_BOARD_Future_1     0x08      // Future
#define VREG_BOARD_Future_2     0x09      // Future

#define VREG_BOARD              0xDFF3FC  // [16-bits] BoardID [HIGH-Byte: MODEL, LOW-Byte: xFREQ]

extern char g_KeepServerRunning;
STRPTR g_DiscoveryMessagePortName __attribute__((aligned(4))) = DISCOVERY_MESSAGE_PORT_NAME;

static void discoveryThread();

static void readApolloOSVer( char *version, LONG len )
{
	BPTR versionFileHandle = Open( "ENV:APOLLOVERSION", MODE_OLDFILE );
	if( versionFileHandle != 0 )
	{
		FGets( versionFileHandle, version, len );
		Close( versionFileHandle );
		return;
	}

	versionFileHandle = Open( "ENV:VBLVERSION", MODE_OLDFILE );
	if( versionFileHandle != 0 )
	{
		FGets( versionFileHandle, version, len );
		Close( versionFileHandle );
	}

	versionFileHandle = Open( "ENV:ABVERSION", MODE_OLDFILE );
	if( versionFileHandle != 0 )
	{
		FGets( versionFileHandle, version, len );
		Close( versionFileHandle );
	}
}

static void ExecVersionToKickstartVersion( UBYTE execVersion, UBYTE *major, UBYTE *minor, UBYTE *rev )
{
	if( execVersion < 31 )
	{
		*major = 1;
		*minor = 0;
		*rev = 0;
		return;
	}
	if( execVersion < 33 )
	{
		*major = 1;
		*minor = 1;
		*rev = 0;
		return;
	}
	if( execVersion < 34 )
	{
		*major = 1;
		*minor = 2;
		*rev = 0;
		return;
	}
	if( execVersion < 36 )
	{
		*major = 1;
		*minor = 3;
		*rev = 0;
		return;
	}
	if( execVersion == 36 )
	{
		*major = 2;
		*minor = 0;
		*rev = 0;
		return;
	}
	if( execVersion == 37 )
	{
		*major = 2;
		*minor = 0;
		*rev = 4;
		return;
	}
	if( execVersion == 37 )
	{
		*major = 2;
		*minor = 0;
		*rev = 4;
		return;
	}

	if( execVersion <= 39 )
	{
		*major = 3;
		*minor = 9;
		*rev = 0;
		return;
	}

	if( execVersion == 40 )
	{
		*major = 3;
		*minor = 1;
		*rev = 0;
		return;
	}

	if( execVersion <= 45 )
	{
		*major = 3;
		*minor = 9;
		*rev = 0;
		return;
	}

	if( execVersion == 46 )
	{
		*major = 3;
		*minor = 1;
		*rev = 4;
		return;
	}

	if( execVersion == 47 )
	{
		*major = 3;
		*minor = 2;
		*rev = 0;
		return;
	}

	if( execVersion == 51 )
	{
		//Apollo OS
		*major = 3;
		*minor = 1;
		*rev = 0;
		return;
	}
}


static void getOSVersion( char *version, LONG len )
{
	struct Library *ExecLibrary = NULL;
	UBYTE min=0, maj=0, rev=0;

	//Did we get the DOSBase
	dbglog( "[getOSVersion] Opening Exec Library.\n" );
	ExecLibrary = OpenLibrary( "exec.library", 0 );
	if( ExecLibrary != NULL )
	{
		memset( version, 0, len );

		//So we can get the exec version now
		dbglog( "[getOSVersion] Exec Library opened.  Exec version: %d.%d\n", ExecLibrary->lib_Version, ExecLibrary->lib_Revision );

		//Lets translate that to a kickstart version
		ExecVersionToKickstartVersion( ExecLibrary->lib_Version, &maj, &min, &rev );

		//ApolloOS stores the version number in ENV
		if( ExecLibrary->lib_Version == 51 )
		{
			//Nothing to do here yet
			readApolloOSVer( version, len );
		}else
		{
			//set the version string
			snprintf( version, len, "%d.%d.%d", maj, min, rev );
			dbglog( "[getOSVersion] OS Version is %s.\n", version );
		}
		CloseLibrary( ExecLibrary );
	}
	dbglog( "[getOSVersion] Done.\n" );
}

static void getOSName( char *name, LONG len )
{
	struct Library *ExecLibrary = NULL;

	//Did we get the DOSBase
	dbglog( "[getOSName] Opening Exec Library.\n" );
	ExecLibrary = OpenLibrary( "exec.library", 0 );
	if( ExecLibrary != NULL )
	{
		//So we can get the exec version now
		dbglog( "[getOSName] Exec Library opened.  Exec version: %d.%d\n", ExecLibrary->lib_Version, ExecLibrary->lib_Revision );

		//AROS has exec version 40.0.  AOS has a variety of numbers
		memset( name, 0, len );
		if( ExecLibrary->lib_Version == 51 /* && ExecLibrary->lib_Revision == 0 */ )
		{
			snprintf( name, len, "%s", "Apollo" );
		}else
		{
			snprintf( name, len, "%s", "AmigaOS" );
		}
		dbglog( "[getOSName] OS Name: %s\n", name );
		CloseLibrary( ExecLibrary );
	}
	dbglog( "[getOSName] Done.\n" );
}

static void getHardwareName( char *name, LONG len )
{
	dbglog( "[getHardwareName] start.\n" );
	//zero the string
	memset( name, 0, len );

	//Vampire boards store their model in a register
	unsigned short *boardRegister = (unsigned short*)0x00DFF3FC;
	UBYTE boardID = ( *boardRegister >> 8 );
	dbglog( "[getHardwareName] board register value: 0x%08X ( 0x%02X).\n", (unsigned int)(*boardRegister), boardID );

	//Now detecht which vampire this is
	switch( boardID )
	{
		case VREG_BOARD_V600:
			snprintf( name, len, "%s", "V2 600" );
			break;
		case VREG_BOARD_V500:
			snprintf( name, len, "%s", "V2 500" );
			break;
		case VREG_BOARD_V4:
			snprintf( name, len, "%s", "Firebird" );
			break;
		case VREG_BOARD_ICEDRAKE:
			snprintf( name, len, "%s", "Icedrake" );
			break;
		case VREG_BOARD_V4SA:
			snprintf( name, len, "%s", "V4 Standalone" );
			break;
		case VREG_BOARD_V1200:
			snprintf( name, len, "%s", "V2 1200" );
			break;
		case VREG_BOARD_V4_V600:
			snprintf( name, len, "%s", "Manticore" );
			break;
		default:
			snprintf( name, len, "%s", "Amiga" );
			break;
	}

	dbglog( "[getHardwareName] Hardware model: %s.\n", name );
	dbglog( "[getHardwareName] end.\n" );
}



void startDiscoveryThread()
{
	if ( DOSBase )
	{
		//Start a client thread
		struct TagItem tags[] __attribute__((aligned(4))) = {
				{ NP_StackSize,		16384 },
				{ NP_Name,			(ULONG)"AEDiscoveryThread" },
				{ NP_Entry,			(ULONG)discoveryThread },
				//{ NP_Output,		(ULONG)consoleHandle },
				{ NP_Synchronous, 	FALSE },
				{ TAG_DONE, 0UL }
		};
		dbglog( "[startDiscoveryThread] Starting discovery thread.\n" );
		struct Process *clientProcess = CreateNewProc(tags);
		if( clientProcess == NULL )
		{
			dbglog( "[startDiscoveryThread] Failed to create client thread.\n" );

			return;
		}
		dbglog( "[startDiscoveryThread] Thread started.\n" );
	}
}

#if 0
static void printInterfaceDebug()
{
	dbglog( "[discoveryThread] Searching for network interfaces.\n" );
	struct List *interfaceList;
	struct Node *node;
	UBYTE interfaceCount = 0;

	//Get the list
	interfaceList = ObtainInterfaceList();
	if( interfaceList == NULL )
	{
		dbglog( "[discoveryThread] unable to get a list of network interfaces.\n" );
		return;
	}
	node = interfaceList->lh_Head;

	while( node )
	{
		if( node->ln_Name == 0 )
			break;
		dbglog( "[discoveryThread] Found interface: %s\n", node->ln_Name );
		node = node->ln_Succ;
		interfaceCount++;
	}
	dbglog( "[discoveryThread] %d network interfaces discovered.\n", interfaceCount );
	dbglog( "[discoveryThread] Network interface discovery completed.\n" );
	ReleaseInterfaceList( interfaceList );
}
#endif

#define ENABLE_DISCOVERY_REPLY 1
static void discoveryThread()
{
	dbglog( "[discoveryThread] Client thread started.\n" );

	struct Library *SocketBase = NULL;
	int returnCode = 0;
	int yes = 1;
	char osVersion[32];
	char osName[32];
	char hardwareName[32];

	//Open the BSD Socket library
	dbglog( "[discoveryThread] Opening bsdsocket.library.\n" );

	//Did we open the bsdsocket library successfully?
	SocketBase = OpenLibrary("bsdsocket.library", 4 );
	if( !SocketBase )
	{
		dbglog( "[discoveryThread] Failed to open the bsdsocket.library.  Exiting.\n" );
		return;
	}

	//printInterfaceDebug();

#if ENABLE_DISCOVERY_REPLY
	//Open a new server port for this client
	dbglog( "[discoveryThread]  Opening client socket.\n" );
	SOCKET discoverySocket = socket(AF_INET, SOCK_DGRAM, 0 );
	if( discoverySocket < 0 )
	{
		dbglog( "[discoveryThread] Error creating UDP Socket.\n" );
		return;
	}

	dbglog( "[discoveryThread] Setting reuse socket options.\n" );
	returnCode = setsockopt( discoverySocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	if(returnCode == SOCKET_ERROR)
	{
		dbglog( "[discoveryThread] Error setting reuse socket options. Exiting.\n" );
		CloseSocket( discoverySocket );
		return;
	}




	//setting the bind port
	dbglog( "[discoverySocket] Preparing to bind to port %d\n", BROADCAST_PORTNUMBER );
	struct sockaddr_in addr __attribute__((aligned(4)));
	memset(&addr, 0, sizeof(addr));
	dbglog( "[discoverySocket] Setting local address\n" );
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons( BROADCAST_PORTNUMBER );
	dbglog( "[discoverySocket] Binding to port %d\n", BROADCAST_PORTNUMBER );
	returnCode = bind( discoverySocket, (struct sockaddr *)&addr, sizeof(addr));
	if(returnCode != 0 )
	{
		dbglog( "[discoverySocket] Unable to bind to port %d. Error Code: %d\n", BROADCAST_PORTNUMBER, returnCode );
		switch( errno )
		{
			case EBADF:
				dbglog( "[discoverySocket] Not a valid descriptor.\n" );
				break;
			case ENOTSOCK:
				dbglog( "[discoverySocket] Not a socket.\n" );
				break;
			case EADDRNOTAVAIL:
				dbglog( "[discoverySocket] Address not available on machine.\n" );
				break;
			case EADDRINUSE:
				dbglog( "[discoverySocket] Address in use.\n" );
				break;
			case EINVAL:
				dbglog( "[discoverySocket] Socket already nound to this address.\n" );
				break;
			case EACCES:
				dbglog( "[discoverySocket] Permission denied.\n" );
				break;
			case EFAULT:
				dbglog( "[discoverySocket] The name parameter is not in a valid part of the user address.\n" );
				break;
			default:
				dbglog( "[discoverySocket] Some other error occurred (%d).\n", returnCode );
				break;
		}
		CloseSocket( discoverySocket );
		//return;
	}
	dbglog( "[discoverySocket] Bound to port %d\n", BROADCAST_PORTNUMBER );
#endif

	//Create a socket for broadcasting
	struct sockaddr_in broadcastAddr;
	memset( &broadcastAddr, 0, sizeof( broadcastAddr ) );
	broadcastAddr.sin_family = AF_INET;
	broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
	broadcastAddr.sin_port = htons( BROADCAST_PORTNUMBER );

	//Create a reply socket for sending the broadcast
	dbglog( "[discoveryThread]  Opening broadcast socket.\n" );
	SOCKET broadcastSocket = socket(AF_INET, SOCK_DGRAM, 0 );
	if( broadcastSocket < 0 )
	{
		dbglog( "[discoveryThread] Error creating broadcast UDP Socket.\n" );

		//Shutdown the discovery socket
#if ENABLE_DISCOVERY_REPLY
		CloseSocket( discoverySocket );
#endif
		return;
	}

	dbglog( "[discoveryThread] Setting reuse socket options.\n" );
	returnCode = setsockopt( broadcastSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	if(returnCode == SOCKET_ERROR)
	{
		dbglog( "[discoveryThread] Error setting reuse socket options. Exiting.\n" );
#if ENABLE_DISCOVERY_REPLY
		CloseSocket( discoverySocket );
#endif
		CloseSocket( broadcastSocket );
		return;
	}

	dbglog( "[discoveryThread] Setting broadcast socket options.\n" );
	returnCode = setsockopt( broadcastSocket, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
	if(returnCode == SOCKET_ERROR)
	{
		dbglog( "[discoveryThread] Error setting broadcast socket options. Exiting.\n" );
#if ENABLE_DISCOVERY_REPLY
		CloseSocket( discoverySocket );
#endif
		CloseSocket( broadcastSocket );
		return;
	}



	//Prepare our messages
	ProtocolMessage_DeviceDiscovery_t *discoveryMessage = AllocVec( sizeof( ProtocolMessage_DeviceDiscovery_t ), MEMF_FAST|MEMF_CLEAR );
	ProtocolMessage_DeviceAnnouncement_t *announceMessage = AllocVec( sizeof( ProtocolMessage_DeviceAnnouncement_t ), MEMF_FAST|MEMF_CLEAR );
	announceMessage->header.token = MAGIC_TOKEN;
	announceMessage->header.type = PMT_DEVICE_ANNOUNCE;
	announceMessage->header.length = sizeof( ProtocolMessage_DeviceAnnouncement_t );
	dbglog( "[discovery] Announcement message length is %u\n", announceMessage->header.length );

	//Add default Values
	strncpy( announceMessage->name, "Unnamed", sizeof( announceMessage->name ) );
	strncpy( announceMessage->osName, "ApolloOS", sizeof( announceMessage->osName ) );
	strncpy( announceMessage->osVersion, "-", sizeof( announceMessage->osVersion ) );
	strncpy( announceMessage->hardware, "Unknown", sizeof( announceMessage->hardware ) );

	//Let's see what is in the tool types
	dbglog( "[discoverySocket] Opening disk object\n" );

	//We need to know the full path of the executable
	BPTR exeDirLock = GetProgramDir();
	char *exeDirPath = AllocVec( 256, MEMF_CLEAR );
	NameFromLock( exeDirLock, exeDirPath, 256 );
	dbglog( "[discoverySocket] executable root dir is %s\n", exeDirPath );

	//WHY DOESNT THIS WORK?
	//We need the name of the exe (becaus some people change it)
	//char exeName[128];
	//GetProgramName( exeName, sizeof( exeName ) );
	//dbglog( "[discoverySocket] executable name is %s\n", exeName );


	//Now form the full file path of the executable
	char *exeFilePath = AllocVec( 256, MEMF_CLEAR );
	snprintf( exeFilePath, 256, "%s/ApolloExplorer", exeDirPath );

	//Try to open the info file using the name
	struct DiskObject *diskObject = GetDiskObject( exeFilePath );
	if( diskObject == NULL )
	{
		snprintf( exeFilePath, 256, "%s/ApolloExplorerSrv", exeDirPath );
		diskObject = GetDiskObject( exeFilePath );
	}
	dbglog( "[discoverySocket] executable file path is %s\n", exeFilePath );
	FreeVec( exeFilePath );
	FreeVec( exeDirPath );

	if( diskObject )
	{
		dbglog( "[discoverySocket] Opened disk object\n" );

		//get the variables we want
		STRPTR name = FindToolType( diskObject->do_ToolTypes, (STRPTR)"name" );
		if( name )
		{
			dbglog( "[discoverySocket] ToolTypes Name: %s\n", name );
			strncpy( announceMessage->name, name, sizeof( announceMessage->name ) );
		}

		//Get the OS Name from Tool Types
		STRPTR osname = FindToolType( diskObject->do_ToolTypes, (STRPTR)"osname" );
		if( osname )
		{
			dbglog( "[discoverySocket] ToolTypes osname: %s\n", osname );
			strncpy( announceMessage->osName, osname, sizeof( announceMessage->name ) );
		}else
		{
			//Ok, there wasn't a name in the tool types.  Let's try and detect it.
			getOSName( osName, sizeof( osName ) );
			if( strlen( osName ) > 0 )
			{
				dbglog( "[discoverySocket] detected osname: %s\n", osName );
				strncpy( announceMessage->osName, osName, sizeof( announceMessage->name ) );
			}
		}

		STRPTR osversion = FindToolType( diskObject->do_ToolTypes, "osversion" );
		if( osversion )
		{
			dbglog( "[discoverySocket] ToolTypes osversion: %s\n", osversion );
			strncpy( announceMessage->osVersion, osversion, sizeof( announceMessage->osVersion ) );
		}else
		{
			getOSVersion( osVersion, sizeof( osVersion ) );
			if( strlen( osVersion ) > 0 )
			{
				dbglog( "[discoverySocket] detected osversion: %s\n", osVersion );
				strncpy( announceMessage->osVersion, osVersion, sizeof( announceMessage->osVersion ) );
			}
		}

		STRPTR hardware = FindToolType( diskObject->do_ToolTypes, (STRPTR)"hardware" );
		if( hardware )
		{
			dbglog( "[discoverySocket] ToolTypes hardware: %s\n", hardware );
			strncpy( announceMessage->hardware, hardware, sizeof( announceMessage->hardware ) );
		}else
		{
			getHardwareName( hardwareName, sizeof( hardwareName ) );
			if( strlen( hardwareName ) > 0 )
			{
				dbglog( "[discoverySocket] detected hardware: %s\n", hardwareName );
				strncpy( announceMessage->hardware, hardwareName, sizeof( announceMessage->hardware ) );
			}
		}
		FreeDiskObject( diskObject );
	}else
	{
		getOSName( osName, sizeof( osName ) );
		if( strlen( osName ) > 0 )
		{
			dbglog( "[discoverySocket] detected osname: %s\n", osName );
			strncpy( announceMessage->osName, osName, sizeof( announceMessage->name ) );
		}
		getOSVersion( osVersion, sizeof( osVersion ) );
		if( strlen( osVersion ) > 0 )
		{
			dbglog( "[discoverySocket] detected osversion: %s\n", osVersion );
			strncpy( announceMessage->osVersion, osVersion, sizeof( announceMessage->osVersion ) );
		}

		getHardwareName( hardwareName, sizeof( hardwareName ) );
		if( strlen( hardwareName ) > 0 )
		{
			dbglog( "[discoverySocket] detected hardware: %s\n", hardwareName );
			strncpy( announceMessage->hardware, hardwareName, sizeof( announceMessage->hardware ) );
		}
	}

	//Make sure that all the strings are NULL terminated correctly
	announceMessage->name[ sizeof( announceMessage->name ) - 1 ] = 0;
	announceMessage->osName[ sizeof( announceMessage->osName ) - 1 ] = 0;
	announceMessage->osVersion[ sizeof( announceMessage->osVersion ) - 1 ] = 0;
	announceMessage->hardware[ sizeof( announceMessage->hardware ) - 1 ]  = 0;


	//Addressing information
	struct sockaddr *requestorAddress = AllocVec( sizeof( *requestorAddress ), MEMF_FAST|MEMF_CLEAR );

	//Setup a message port so we can get kill messages and the like
	dbglog( "[discoverySocket] Creating message port %s.\n", DISCOVERY_MESSAGE_PORT_NAME );
	struct MsgPort *messagePort = CreateMsgPort();
	if( messagePort == NULL )
	{
		dbglog( "Failed to create message port.\n" );
		goto shutdown;
	}
	messagePort->mp_Node.ln_Name = g_DiscoveryMessagePortName;
	messagePort->mp_Node.ln_Pri = 0;
	AddPort( messagePort );
	dbglog( "[discoverySocket] Master Message Port: 0x%08x\n", (unsigned int)messagePort );

	//Start reading all in-bound messages
	int bytesRead = 0;
	int bytesSent = 0;
	char keepThisConnectionRunning = 1;
	LONG bytesAvailable = 0;
	while( g_KeepServerRunning && keepThisConnectionRunning )
	{
		//Check if we got an exit message
		struct Message *newMessage = GetMsg( messagePort );
		if( newMessage != NULL )
		{
			struct AEMessage *aeMsg = (struct AEMessage *)newMessage;
			if( aeMsg->messageType == AEM_Shutdown )
			{
				dbglog( "[discovery] Got the shutdown command. Shutting down.\n" );
				
				//Send a reply
				dbglog( "[discovery] Acknowledging master's request.\n" );
				ReplyMsg( newMessage );

				//Now get out of here
				dbglog( "[discovery] Now initiating a shutdown.\n" );
				goto shutdown; 
			}else
			{
				dbglog( "[discovery] Got some unknown message.\n" );
			}
		}

#if ENABLE_DISCOVERY_REPLY
		socklen_t requestorAddressLen = sizeof( *requestorAddress );
		memset( requestorAddress, 0, requestorAddressLen );
		IoctlSocket( discoverySocket,FIONREAD ,&bytesAvailable );
		if( bytesAvailable != 0 )
		{
			bytesRead = recvfrom( 	discoverySocket,
									discoveryMessage,
									sizeof( ProtocolMessage_DeviceDiscovery_t ),
									0,
									(struct sockaddr *)requestorAddress,
									&requestorAddressLen );

			//If the datagram isn't the right size, ignore it.
			if( bytesRead != sizeof( ProtocolMessage_DeviceDiscovery_t ) )
			{
				dbglog( "[discoverySocket] Ignoring packet which was only %d bytes in size.\n", bytesRead );
				continue;
			}

			//Ignore invalid tokens
			if( discoveryMessage->header.token != MAGIC_TOKEN )
			{
				dbglog( "[discoverySocket] Ignoring packet with invalid token 0x%08x.\n", discoveryMessage->header.token );
				continue;
			}

			//Ignore invalid types
			if( discoveryMessage->header.type != PMT_DEVICE_DISCOVERY )
			{
				dbglog( "[discoverySocket] Ignoring packet with invalid type 0x%08x.\n", discoveryMessage->header.type );
				continue;
			}

			//If we got this far, then we should send the device info
			//dbglog( "[discoverySocket] Sending reply to device discovery.\n" );
			bytesSent = sendto( discoverySocket, announceMessage, announceMessage->header.length, 0, requestorAddress, requestorAddressLen );
		}
#endif
		//dbglog( "[discoverySocket] Sent %d bytes.\n", bytesSent );
		bytesSent = sendto( broadcastSocket, announceMessage, announceMessage->header.length, 0, (struct sockaddr *)&broadcastAddr, sizeof( broadcastAddr ) );
		//dbglog( "[discoverySocket] Sent %d bytes.\n", bytesSent );
		(void)bytesSent;


		//To prevent overloading the amiga....
		Delay( 50 );
	}

	shutdown:
	dbglog( "[discovery] Shutting down.\n" );
	Delay( 100 );

	//Clear out and then free the message port
	dbglog( "[discovery] Removing message port.\n" );
	struct Message *newMessage = GetMsg( messagePort );
	while( newMessage )
	{
		newMessage = GetMsg( messagePort );
	}
	RemPort( messagePort );
	DeleteMsgPort( messagePort );

	//Free our messagebuffer
	dbglog( "[discovery] Freeing memory.\n" );
	FreeVec( discoveryMessage );
	FreeVec( announceMessage );
	FreeVec( requestorAddress );

#if ENABLE_DISCOVERY_REPLY
	//Now close the socket because we are done here
	dbglog( "[discoverySocket] Closing client thread for socket 0x%08x.\n", discoverySocket );
	CloseSocket( discoverySocket );
#endif

	dbglog( "[discoverySocket] Closing client thread for socket 0x%08x.\n", broadcastSocket );
	CloseSocket( broadcastSocket );

	dbglog( "[discovery] Closing socket base.\n" );
	CloseLibrary( SocketBase );

	return;
}
