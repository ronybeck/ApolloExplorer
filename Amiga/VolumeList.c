/*
 * VolumeList.c
 *
 *  Created on: May 28, 2021
 *      Author: rony
 */

#include "VolumeList.h"

#define DBGOUT 0

#ifdef __GNUC__
#if DBGOUT
#include <stdio.h>
#endif
#include <string.h>
#include <proto/dos.h>
#include <proto/exec.h>
#endif

#ifdef __VBCC__

#endif

#include "VNetUtil.h"


ProtocolMessage_VolumeList_t *getVolumeList()
{
	unsigned int volumeNameLength = 0;
	char volumeName[ 128 ] = "";

	dbglog( "[getVolumeList] Getting the list of volumes.\n" );

	//Lock the dos list
	struct DosList *dosList = LockDosList( LDF_VOLUMES|LDF_READ );
	if( dosList == NULL )
	{
		dbglog( "[getVolumeList] Unable to get DOSList.\n" );
		return NULL;
	}else
	{
		dbglog( "[getVolumeList] Got a DOSList.\n" );
	}
	unsigned int volumeCount = 0;
	unsigned int stringSize = 0;
	while ( dosList )
	{
		volumeNameLength = AROS_BSTR_strlen( dosList->dol_Name );
		if( volumeNameLength > sizeof( volumeName ) )
		{
			dbglog( "[getVolumeList] A volume name was larger than 128 bytes (%d).  This should never be!  Ignoring.\n", volumeNameLength );

			//Get the next from the dosList
			dosList = NextDosEntry( dosList, LDF_VOLUMES );
			continue;
		}
		CopyMem( AROS_BSTR_ADDR( dosList->dol_Name ), volumeName, volumeNameLength );
		volumeName[ volumeNameLength ] = 0;		//Terminating 0 because I don't trust that there is one.
		if( dosList->dol_Type != DLT_VOLUME )
		{
			//dbglog( "[getVolumeList] Ignoring entry '%s' which is of type 0x%08x.\n", volumeName, dosList->dol_Type );
		}else
		{
			dbglog( "[getVolumeList] Adding entry '%s' (size %d) which is of type 0x%08x.\n", volumeName, volumeNameLength, (unsigned int)dosList->dol_Type );
			volumeCount++;
			stringSize += volumeNameLength;
		}

		//Get the next from the dosList
		dosList = NextDosEntry( dosList, LDF_VOLUMES );
	}
	UnLockDosList( LDF_VOLUMES|LDF_READ );

	dbglog( "[getVolumeList] There are %d volumes.  %d bytes are required to store the strings.\n", volumeCount, stringSize );

	//Now create a the Volume List message
	unsigned int messageLength = sizeof( ProtocolMessage_VolumeList_t ) + ( volumeCount * sizeof( VolumeEntry_t ) ) + stringSize;
	ProtocolMessage_VolumeList_t *vlistMessage = (ProtocolMessage_VolumeList_t*)AllocVec( messageLength, MEMF_FAST|MEMF_CLEAR );
	vlistMessage->header.token = MAGIC_TOKEN;
	vlistMessage->header.type = PMT_VOLUME_LIST;
	vlistMessage->header.length = messageLength;
	vlistMessage->volumeCount = volumeCount;

	dbglog( "[getVolumeList] volumeListMessage is %d bytes in size and is at address 0x%08x.\n", vlistMessage->header.length, (unsigned int)vlistMessage );
	dbglog( "[getVolumeList] volumeListMessage occupies addresses 0x%08x - 0x%08x.\n", (unsigned int)vlistMessage, (unsigned int)((char*)vlistMessage + messageLength) );

	//Go through the list again and create the message
	dosList = LockDosList( LDF_VOLUMES|LDF_READ );
	VolumeEntry_t *volumeEntry = vlistMessage->volumes;
	dbglog( "[getVolumeList] VolumeEntry address: 0x%08x.\n", (unsigned int)volumeEntry );
	while ( dosList )
	{
		volumeNameLength = AROS_BSTR_strlen( dosList->dol_Name );
		if( volumeNameLength > sizeof( volumeName ) )
		{
			dbglog( "[getVolumeList] A volume name was larger than 128 bytes (%d).  This should never be!  Ignoring.\n", volumeNameLength );

			//Get the next from the dosList
			dosList = NextDosEntry( dosList, LDF_VOLUMES );
			continue;
		}
		CopyMem( AROS_BSTR_ADDR( dosList->dol_Name ), volumeName, volumeNameLength );
		volumeName[ volumeNameLength ] = 0;		//Terminating 0 because I don't trust that there isn't one.
		//dbglog( "[getVolumeList] Measured Volume string length is %d.\n", strlen( volumeName ) );
		if( dosList->dol_Type != DLT_VOLUME )
		{
			//dbglog( "[getVolumeList] Ignoring entry '%s' which is of type 0x%08x.\n", volumeName, dosList->dol_Type );
		}else
		{
			dbglog( "[getVolumeList] ================================================================\n" );
			dbglog( "[getVolumeList] VolumeEntry address: 0x%08x.\n", (unsigned int)volumeEntry );
			dbglog( "[getVolumeList] Adding entry '%s' which is of type 0x%08x.\n", volumeName, (unsigned int)dosList->dol_Type );

			//Create the new entry
			volumeEntry->entrySize = sizeof( VolumeEntry_t ) + volumeNameLength;	//We don't need the extra char for terminating 0 in the size calculation
			strncpy( volumeEntry->name, volumeName, volumeNameLength + 1 );
			volumeEntry->name[ volumeNameLength ] = 0; //Don't trust that this won't be the case.
			volumeEntry->diskType = dosList->dol_misc.dol_volume.dol_DiskType;

			//Prepare the next entry
			dbglog( "[getVolumeList] Strlen is: %d.\n", volumeNameLength );
			dbglog( "[getVolumeList] Entry size is: %d (%x) .\n", volumeEntry->entrySize, volumeEntry->entrySize  );
			volumeEntry = (VolumeEntry_t*)(((char*)volumeEntry) + ((char)volumeEntry->entrySize));
			dbglog( "[getVolumeList] New volumeEntry address: 0x%08x.\n", (unsigned int)volumeEntry );
			dbglog( "[getVolumeList] ================================================================\n" );
		}

		//Get the next from the dosList
		dosList = NextDosEntry( dosList, LDF_VOLUMES );
	}
	UnLockDosList( LDF_VOLUMES|LDF_READ );

	//Now return this message
	return vlistMessage;
}
