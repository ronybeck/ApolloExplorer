/*
 * VNetServerThread.h
 *
 *  Created on: May 16, 2021
 *      Author: rony
 */

#ifndef VNETSERVERTHREAD_H_
#define VNETSERVERTHREAD_H_

#include "protocol.h"

void startClientThread( struct Library *SocketBase, struct MsgPort *msgPort, SOCKET clientSocket );
//void startDiscoveryThread();

#endif /* VNETSERVERTHREAD_H_ */
