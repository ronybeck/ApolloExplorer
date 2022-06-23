/*
 * AETypes.h
 *
 *  Created on: Jun 12, 2022
 *      Author: rony
 */

#ifndef AMIGA_AETYPES_H_
#define AMIGA_AETYPES_H_

typedef enum
{
	AEM_NewClient,
	AEM_ClientThreadTerminating,
	AEM_KillClient,
	AEM_Shutdown,
	AEM_None
} AEMessgeType_t;


struct AEMessage
{
	struct Message msg;
	unsigned short port;
	AEMessgeType_t messageType;
};


#endif /* AMIGA_AETYPES_H_ */
