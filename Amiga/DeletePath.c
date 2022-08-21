#include "DeletePath.h"

#include <string.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>

#define DBGOUT 1
#include "AEUtil.h"
#include "protocolTypes.h"
#define BNULL 0


ProtocolMessage_PathDeleted_t *g_PathDelMsg = NULL;
DeleteFailureReason g_FailureReason;
char g_CurrentPath[ MAX_FILEPATH_LENGTH * 2];    //If there is an operation underway, this will be populated.
struct AnchorPath *g_AnchorPath = NULL;
BPTR g_DirLock = 0;

static void initialiseDelete()
{
    ULONG msgSize __attribute__((aligned(4))) = sizeof( *g_PathDelMsg ) + (MAX_FILEPATH_LENGTH*3);  //We allocate enough space for unexpectedly long paths
    ULONG anchorSize __attribute__((aligned(4))) = sizeof( *g_AnchorPath ) + (MAX_FILEPATH_LENGTH*3);
    //Allocate the message
    if( g_PathDelMsg == NULL )
    {
        g_PathDelMsg = AllocVec( msgSize, MEMF_FAST );

        //Set up the message
        g_PathDelMsg->header.type = PMT_PATH_DELETED;
        g_PathDelMsg->header.length = sizeof( *g_PathDelMsg );  //This needs to be overwritten on a per-path basis
        g_PathDelMsg->header.token = MAGIC_TOKEN;
    }

    //Allocate the anchor
    if( g_AnchorPath == NULL )
    {
        g_AnchorPath = AllocVec( anchorSize, MEMF_FAST );
    }

    //Just to be on the safe side, clear out the memory structs
    memset( g_PathDelMsg, 0, msgSize );
    memset( g_AnchorPath, 0, anchorSize );
    memset( g_CurrentPath, 0, sizeof( g_CurrentPath ) );
    g_AnchorPath->ap_Strlen = MAX_FILEPATH_LENGTH;
}

BOOL inline isDeletable( struct	FileInfoBlock *fib )
{
	if( fib->fib_Protection & FIBF_DELETE )
		return FALSE;
	return TRUE;
}


ProtocolMessage_PathDeleted_t *deleteFile( char *path )
{
	dbglog( "[deleteFile] Deleting file '%s'\n", path );
    //First check that the string is valid
    if( path == NULL || ( strlen( path ) > MAX_FILEPATH_LENGTH ) )
    {
        //Well we are already carrying out an operation
        g_PathDelMsg->deleteSucceeded = 0;
        g_PathDelMsg->failureReason = DFR_PATH_NOT_FOUND;
        g_PathDelMsg->deleteCompleted = 0;
        g_PathDelMsg->filePath[ 0 ] = 0;
        g_PathDelMsg->header.length = sizeof( *g_PathDelMsg );
        g_PathDelMsg->header.type = PMT_PATH_DELETED;
        g_PathDelMsg->header.token = MAGIC_TOKEN;
        return g_PathDelMsg;
    }

    //First, check that we aren't in a current operation
    if( strlen( g_CurrentPath ) )
    {
        //Well we are already carrying out an operation
        g_PathDelMsg->deleteSucceeded = 0;
        g_PathDelMsg->failureReason = DFR_OPERATION_ALREADY_UNDERWAY;
        g_PathDelMsg->deleteCompleted = 0;
        g_PathDelMsg->header.type = PMT_PATH_DELETED;
        g_PathDelMsg->header.token = MAGIC_TOKEN;
        g_PathDelMsg->filePath[ 0 ] = 0;
        g_PathDelMsg->header.length = sizeof( *g_PathDelMsg );
        return g_PathDelMsg;
    }

    //Initialise the delete (if we didn't already)
    initialiseDelete();

    //Do the delete
	if( DeleteFile( path ) )
	{
		dbglog( "[deleteFile] Deleted file '%s'\n", path );
		g_PathDelMsg->deleteSucceeded = 1;
		g_PathDelMsg->failureReason = DFR_NONE;
		g_PathDelMsg->deleteCompleted = 1;
		g_PathDelMsg->header.type = PMT_PATH_DELETED;
		g_PathDelMsg->header.token = MAGIC_TOKEN;
		g_PathDelMsg->header.length = sizeof( *g_PathDelMsg );
		g_PathDelMsg->filePath[ 0 ] = 0;
		return g_PathDelMsg;
	}

    //Else we had some sort of missmatch
    g_FailureReason = DFR_UNKNOWN;
    return getDeleteError();
}

char startRecursiveDelete( char* path )
{
    //First check that the string is valid
    if( path == NULL || strlen( path ) > MAX_FILEPATH_LENGTH )
    {
        //Well we are already carrying out an operation
        g_FailureReason = DFR_PATH_NOT_FOUND;
        memset( g_CurrentPath, 0, sizeof( g_CurrentPath ) );
        dbglog( "[startRecursiveDelete] Path invalid or missing.\n" );
        return 1;
    }

    //First, check that we aren't in a current operation
    if( strlen( g_CurrentPath ) )
    {
        //Well we are already carrying out an operation
        g_FailureReason = DFR_OPERATION_ALREADY_UNDERWAY;
        dbglog( "[startRecursiveDelete] Delete is already underway.\n" );
        return 1;
    }

    //Initialise the deletion
    initialiseDelete();
    strncpy( g_CurrentPath, path, sizeof( g_CurrentPath ) - 1 );
    g_FailureReason = DFR_NONE;

    //Make sure that the string has terminating "/"
    if( 	g_CurrentPath[ strlen( g_CurrentPath ) - 1 ] == ':' &&
    		g_CurrentPath[ strlen( g_CurrentPath ) - 1 ] == '/')
    {
    	g_CurrentPath[ strlen( g_CurrentPath ) ] = '/';
    }

    //Now find the first match
    dbglog( "[startRecursiveDelete] Starting recursive delete for path %s\n", g_CurrentPath );
    LONG returnCode = MatchFirst( g_CurrentPath, g_AnchorPath );
    if( returnCode > 0 )
    {
    	//TODO: Find all the error scenarios here
    	dbglog( "[startRecursiveDelete] Failed to find the first match for path %s.\n", g_CurrentPath );
    	g_FailureReason = DFR_UNKNOWN;
    	MatchEnd( g_AnchorPath );
    	return 1;
    }
    g_AnchorPath->ap_Flags |= APF_DODIR; //We want to recurse down through this dir

    return 0;
}

ProtocolMessage_PathDeleted_t *getDeleteError()
{
    //Form the message based on the last operations
    g_PathDelMsg->deleteSucceeded = g_FailureReason > DFR_NONE ? 0 : 1;
    g_PathDelMsg->deleteCompleted = 0;
    g_PathDelMsg->failureReason = g_FailureReason;
    g_PathDelMsg->header.type = PMT_PATH_DELETED;
    g_PathDelMsg->header.token = MAGIC_TOKEN;
    g_PathDelMsg->header.length = sizeof( *g_PathDelMsg );
    g_PathDelMsg->filePath[ 0 ] = 0;
    //g_PathDelMsg->header.length = sizeof( *g_PathDelMsg ) + strlen( g_CurrentPath );
    return g_PathDelMsg;
}

ProtocolMessage_PathDeleted_t *getNextFileDeleted()
{
	//Now get the first entry in this directory
	//dbglog( "[getNextFileDeleted] Get the next match\n" );
	LONG returnCode = MatchNext( g_AnchorPath );
	if( returnCode > 0 )
	{
		dbglog( "[getNextFileDeleted] Failed to get the next match.  No more entries?\n" );
		g_FailureReason = DFR_UNKNOWN;
		memset( g_CurrentPath, 0, sizeof( g_CurrentPath ) );
		return NULL;
	}

	//Copy the path to our globals
	strncpy( g_CurrentPath, g_AnchorPath->ap_Buf, MAX_FILEPATH_LENGTH );

	dbglog( "[getNextFileDeleted] Path %s ", g_AnchorPath->ap_Buf );
	switch( g_AnchorPath->ap_Info.fib_EntryType )
	{
		case DET_USERDIR:
		{
			dbglog( "directory");
			if( g_AnchorPath->ap_Flags & APF_DIDDIR  )
			{
				//We want to delete this now, so lock the parent directory
				char dirPath[ MAX_FILEPATH_LENGTH ] __attribute__((aligned(4)));
				strncpy( dirPath, g_AnchorPath->ap_Buf, MAX_FILEPATH_LENGTH );
				char *endOfPath = PathPart( dirPath );
				*endOfPath=0;	//null terminate it so we have the path
				if( g_DirLock != BNULL )	UnLock( g_DirLock );
				g_DirLock = Lock( dirPath, ACCESS_WRITE );

				g_AnchorPath->ap_Flags &= ~APF_DIDDIR;
				dbglog( " - deleting ");

				//Check if the file is delete protected
				if( !isDeletable( &g_AnchorPath->ap_Info ) )
				{
					dbglog( "[getNextFileDeleted] removing delete protection from %s\n", g_AnchorPath->ap_Buf );
					SetProtection( g_AnchorPath->ap_Buf, 0 );
				}

				if( DeleteFile( g_AnchorPath->ap_Buf ) )
				{
					dbglog( " - success ");
					g_PathDelMsg->deleteSucceeded = 1;
					g_PathDelMsg->failureReason = DFR_NONE;
				}
				else
				{
					dbglog( " - failed " );
					g_PathDelMsg->deleteSucceeded = 0;
					g_PathDelMsg->failureReason = DFR_UNKNOWN;
				}
				if( g_DirLock != BNULL )	UnLock( g_DirLock );
				g_DirLock = BNULL;
			}
			else
			{
				dbglog( " - entering ");
				g_AnchorPath->ap_Flags |= APF_DODIR;
			}
			break;
		}
		case DET_FILE:
		{
			dbglog( "file" );
			dbglog( " - deleting ");

			//Check if the file is delete protected
			if( !isDeletable( &g_AnchorPath->ap_Info ) )
			{
				dbglog( "[getNextFileDeleted] removing delete protection from %s\n", g_AnchorPath->ap_Buf );
				SetProtection( g_AnchorPath->ap_Buf, 0 );
			}

			if( DeleteFile( g_AnchorPath->ap_Buf ) )
			{
				dbglog( " - success " );
				g_PathDelMsg->deleteSucceeded = 1;
				g_PathDelMsg->failureReason = DFR_NONE;
			}
			else
			{
				dbglog( " - failed " );
				g_PathDelMsg->deleteSucceeded = 0;
				g_PathDelMsg->failureReason = DFR_UNKNOWN;
			}
			break;
		}
		default:
			dbglog( "other" );
			break;
	}
	dbglog( "\n" );

	//Form the response message
	//dbglog( "[getNextFileDeleted] completed\n" );
	g_PathDelMsg->deleteCompleted = 0;
    g_PathDelMsg->header.type = PMT_PATH_DELETED;
    g_PathDelMsg->header.token = MAGIC_TOKEN;
	g_PathDelMsg->header.length = sizeof( *g_PathDelMsg ) + strlen( g_CurrentPath );
	strncpy( g_PathDelMsg->filePath, g_CurrentPath, MAX_FILEPATH_LENGTH );
	return g_PathDelMsg;
}

char endRecursiveDelete()
{
	MatchEnd( g_AnchorPath );
	memset( g_CurrentPath, 0, sizeof( g_CurrentPath ) );
	if( g_DirLock != BNULL )UnLock( BNULL );
	g_DirLock = BNULL;
	return 0;
}


ProtocolMessage_PathDeleted_t *getRecusiveDeleteCompletedMessage()
{
	g_PathDelMsg->deleteSucceeded = 1;
	g_PathDelMsg->deleteCompleted = 1;
	g_PathDelMsg->failureReason = DFR_NONE;
	g_PathDelMsg->filePath[0] = '\0';
    g_PathDelMsg->header.type = PMT_PATH_DELETED;
    g_PathDelMsg->header.token = MAGIC_TOKEN;
	g_PathDelMsg->header.length = sizeof( *g_PathDelMsg );
	return g_PathDelMsg;
}

void cleanDeletePathGlobals()
{
    if( g_PathDelMsg != NULL )  FreeVec( g_PathDelMsg );
    if( g_AnchorPath != NULL )  FreeVec( g_AnchorPath );
}
