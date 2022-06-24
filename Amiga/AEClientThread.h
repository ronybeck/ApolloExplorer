/*
 * AEClientThread.h
 *
 *  Created on: May 16, 2021
 *      Author: rony
 */

#ifndef AECLIENTTHREAD_H_
#define AECLIENTTHREAD_H_

#include "protocol.h"

typedef struct clientThread
{
	struct clientThread *next;
	struct clientThread *previous;
	struct Process *process;
	struct MsgPort *messagePort;
	char ip[4];
	UWORD port;
} ClientThread_t;

void startClientThread( struct Library *SocketBase, struct MsgPort *msgPort, SOCKET clientSocket );

void addClientThreadToList( ClientThread_t *client );
void removeClientThreadFromList( ClientThread_t *client );
ClientThread_t *getClientThreadList();
void initialiseClientThreadList();
void freeClientThreadList( ClientThread_t *list );
void lockClientThreadList();
void unlockClientThreadList();
UBYTE getClientListSize();
void getIPFromClient( ClientThread_t *client, char ipAddress[ 21 ] );

#endif /* AECLIENTTHREAD_H_ */
