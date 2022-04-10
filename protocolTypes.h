/*
 * protocolTypes.h
 *
 *  Created on: May 10, 2021
 *      Author: rony
 */

#ifndef PROTOCOLTYPES_H_
#define PROTOCOLTYPES_H_

#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define VERSION_REVISION 5
#define MAIN_LISTEN_PORTNUMBER 30202
#define BROADCAST_PORTNUMBER 30201


#define COMMAND_SEND_FILE	0x00000000
#define COMMAND_RUN			0x00000001
#define COMMAND_CLOSE		0x00000002
#define COMMAND_MKDIR		0x00000003
#define COMMAND_DIR			0x00000004
#define COMMAND_GET_FILE	0x00000005

#define MAGIC_TOKEN		0xAABBCCDD
#define MAX_MESSAGE_LENGTH 0x30000
#define FILE_CHUNK_SIZE 0x8000
#define MAX_FILEPATH_LENGTH 256

typedef enum
{
	//New Connection Signalling
    PMT_NEW_CLIENT_PORT     = 0x00000001,
    PMT_VERSION             = 0x00000002,
    PMT_GET_VERSION         = 0x00000003,
    PMT_SHUTDOWN_SERVER     = 0x00000004,
    PMT_ACK                 = 0x00000005,
    PMT_CANCEL_OPERATION    = 0x00000006,
	PMT_FAILED				= 0x00000007,

	//File operations
    PMT_START_OF_SEND_FILE  = 0x00000010,
    PMT_FILE_CHUNK          = 0x00000011,
    PMT_CLOSE               = 0x00000012,
    PMT_MKDIR               = 0x00000013,
    PMT_GET_DIR_LIST        = 0x00000014,
    PMT_DIR_LIST            = 0x00000015,
    PMT_GET_FILE            = 0x00000016,
	PMT_PUT_FILE			= 0x00000017,
	PMT_DELETE_PATH			= 0x00000018,
	PMT_GET_VOLUMES			= 0x00000019,
	PMT_VOLUME_LIST			= 0x0000001A,

	//Device Discovery
	PMT_DEVICE_DISCOVERY	= 0x00000020,
	PMT_DEVICE_ANNOUNCE		= 0x00000021,

	//remote commands
    PMT_RUN                 = 0x00000021,
	PMT_REBOOT				= 0x00000022,
    PMT_INVALID             = 0x00000023
} ProtocolMessageType_t;

typedef struct
{
	unsigned int token;				//This token must be present at the begining of every message
	unsigned int type;				//The ProtocolMessageType_t
	unsigned int length;			//The length of this message including the payload
} ProtocolMessage_t;

typedef struct
{
    ProtocolMessage_t header;
	char response;					//User defined by 0 reject and !0 is accepted generally speaking
	char padding[ 3 ];
} ProtocolMessage_Ack_t;


//This is a signal from the server informing the client to reconnect on a new port
typedef struct
{
	ProtocolMessage_t header;
	unsigned short port;			//The new port for this client.
} ProtocolMessageNewClientPort_t;


typedef enum
{
	DET_ROOT 	= 0x00000001,
	DET_USERDIR	= 0x00000002,
	DET_SOFTLINK= 0x00000003,
	DET_LINKDIR	= 0x00000004,
	DET_FILE	= 0xFFFFFFFD,
	DET_LINKFILE= 0xFFFFFFFC,
	DET_PIPEFILE= 0xFFFFFFFB
} DirEntType_t;

typedef struct
{
	unsigned int entrySize;			//The size of this entry
	unsigned int type;				//See DirEntType_t
	unsigned int size;				//File size in bytes
	//unsigned int filenameLength;	//The length of the filename
	char filename[1];				//The filename as a null terminated c-string
} ProtocolMessage_DirEntry_t;

typedef struct
{
	ProtocolMessage_t header;
	unsigned int entryCount;
	ProtocolMessage_DirEntry_t entries[1];
} ProtocolMessageDirectoryList_t;

typedef struct
{
	ProtocolMessage_t header;
	unsigned int length;
	char path[1];
} ProtocolMessageGetDirectoryList_t;

typedef struct
{
	ProtocolMessage_t header;
    char reason[ 128 ];
} ProtocolMessage_QuitServer_t;

typedef struct
{
	ProtocolMessage_t header;
	unsigned char major;
	unsigned char minor;
	unsigned char rev;
} ProtocolMessage_Version_t;

typedef struct
{
	ProtocolMessage_t header;
	char filePath[1];
} ProtocolMessage_FilePull_t;

typedef struct
{
	ProtocolMessage_t header;
	char filePath[1];
} ProtocolMessage_FilePut_t;

typedef struct
{
	ProtocolMessage_t header;
	unsigned int fileSize;
	unsigned int numberOfFileChunks;
	char filePath[1];
} ProtocolMessage_StartOfFileSend_t;

typedef struct
{
	ProtocolMessage_t header;
	unsigned int chunkNumber;
    unsigned int bytesContained;
    char chunk[ FILE_CHUNK_SIZE ];
} ProtocolMessage_FileChunk_t;

typedef struct
{
	ProtocolMessage_t header;
	char filePath[1];
} ProtocolMessage_MakeDir_t;

typedef struct
{
	ProtocolMessage_t header;
	char filePath[1];
} ProtocolMessage_DeletePath_t;

typedef struct
{
	ProtocolMessage_t header;
} ProtocolMessage_GetVolumeList_t;

typedef struct
{
	unsigned int entrySize;
	unsigned int diskType;
	char name[1];
} VolumeEntry_t;

typedef struct
{
	ProtocolMessage_t header;
	unsigned int volumeCount;
	VolumeEntry_t volumes[ 1 ];
} ProtocolMessage_VolumeList_t;

typedef struct
{
	ProtocolMessage_t header;
	char command[1];
} ProtocolMessage_Run_t;

typedef struct
{
	ProtocolMessage_t header;
} ProtocolMessage_DeviceDiscovery_t;

typedef struct
{
	ProtocolMessage_t header;
	char name[16];
	char osName[8];
	char osVersion[8];
	char hardware[8];
	char online;			//1 online, 0 going offline
	char padding[3];
} ProtocolMessage_DeviceAnnouncement_t;

typedef struct
{
	ProtocolMessage_t header;
} ProtocolMessage_Reboot_t;

typedef struct
{
	ProtocolMessage_t header;
} ProtocolMessage_CancelOperation_t;

typedef struct
{
	ProtocolMessage_t header;
	char message[1];
} ProtocolMessage_Failed_t;

#endif /* PROTOCOLTYPES_H_ */
