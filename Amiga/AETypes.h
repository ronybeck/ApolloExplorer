/*
 * AETypes.h
 *
 *  Created on: Jun 12, 2022
 *      Author: rony
 */

#ifndef AMIGA_AETYPES_H_
#define AMIGA_AETYPES_H_

#define WB_ICON_DOUBLE_CLICKED 0x00080000

typedef enum
{
	AEM_NewClient,
	AEM_KillClient,
	AEM_Shutdown,
	AEM_ClientList,
	AEM_None
} AEMessgeType_t;


struct AEMessage
{
	struct Message msg;
	AEMessgeType_t messageType;
	unsigned short port;
};

struct AEClientList
{
	struct Message msg;
	AEMessgeType_t messageType;
	char clientCount;
	char ipAddress[21];
	short port;
};


#endif /* AMIGA_AETYPES_H_ */
