#define DBGOUT 0

#include <sys/types.h>
#include <clib/dos_protos.h>

#include "protocolTypes.h"
#include "protocol.h"
#include "VNetUtil.h"
#include "VNetServerThread.h"
#include "VNetDiscoveryThread.h"

struct Library *SocketBase = NULL;

/*****************************************************************************/

#include <proto/exec.h>
#define __BSDSOCKET_NOLIBBASE__
#include <proto/bsdsocket.h>

char g_KeepServerRunning = 1;

char* g_MessagePortName = MASTER_MSGPORT_NAME;
struct MsgPort *g_VNetSocketMessagePort = NULL;

/*****************************************************************************/

int main(int argc, char *argv[])
{
	struct sockaddr_in addr;
	//SOCKET s = 0;
	SOCKET serverSocket = 0;
	int res = 0;

	//We want to do at least a few attempts to start this
	//Maybe the TCP/Stack is starting in the back ground
	//and we need to give it a few seconds to start
	int numberOfRestartAttempts = 15;

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

	dbglog( "Setting socket options.\n" );
	int yes = 1;
	res = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	if(res == SOCKET_ERROR)
	{
		dbglog("setsockopt error\n");
		return 1;
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

#if 0
	//Create a message port so we can send the socket handles to child processes
	dbglog( "[master] Creating message port %s.\n", g_MessagePortName );
	g_VNetSocketMessagePort = CreatePort( g_MessagePortName, 0 );
	if( g_VNetSocketMessagePort == NULL )
	{
		dbglog( "Failed to create message port.\n" );
		goto shutdown;
	}
	AddPort( g_VNetSocketMessagePort );
	dbglog( "[master] Master Message Port: 0x%08x\n", g_VNetSocketMessagePort );
#endif

#if 1
	//Create a message port so we can send the socket handles to child processes
	dbglog( "[master] Creating message port %s.\n", g_MessagePortName );
	g_VNetSocketMessagePort = CreateMsgPort();
	if( g_VNetSocketMessagePort == NULL )
	{
		dbglog( "Failed to create message port.\n" );
		goto shutdown;
	}
	g_VNetSocketMessagePort->mp_Node.ln_Name = g_MessagePortName;
	g_VNetSocketMessagePort->mp_Node.ln_Pri = 0;
	AddPort( g_VNetSocketMessagePort );
	dbglog( "[master] Master Message Port: 0x%08x\n", (unsigned int)g_VNetSocketMessagePort );
#endif

	//Start announcing our presence on the network
	startDiscoveryThread();

	while( g_KeepServerRunning )
	{
		dbglog("Awaiting new connection\n");
		socklen_t addrLen = sizeof( addr );
		SOCKET newClientSocket = (SOCKET)accept(serverSocket, (struct sockaddr *)&addr, &addrLen);
		if( newClientSocket < 0 )
		{
			dbglog( "accept error %d \"%s\"\n", errno, strerror( errno ) );
			break;
		}

		//Now spawn a client thread
		dbglog( "Starting client thread for new socket 0x%08x\n", newClientSocket );
		startClientThread( SocketBase, g_VNetSocketMessagePort, newClientSocket );
	}

	shutdown:

	//Clean up
	if( g_VNetSocketMessagePort )
	{
		RemPort( g_VNetSocketMessagePort );
		DeleteMsgPort( g_VNetSocketMessagePort );
	}
	CloseSocket(serverSocket);

	return 0;
}




