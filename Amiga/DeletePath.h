/*
 * DeletePath.h
 *
 *  Created on: August, 2022
 *      Author: rony
 */

#ifndef AMIGA_DELETEPATH_H_
#define AMIGA_DELETEPATH_H_

#include "protocolTypes.h"

/**
 * @brief Delete a single file
 * 
 * @param path The full path to the file to be deleted
 * @return ProtocolMessage_PathDeleted_t* A response containing either a code indicating success or a code indicating failure and which failure should the file not be deleted.
 */
ProtocolMessage_PathDeleted_t *deleteFile( char *path );

/**
 * @brief Start a recursive deletion of the directory and all files and subdirectories.  The server will go down through the directory
 * structure and delete all files and directories that it finds.
 * 
 * @param path The path to the toplevel of the path (i.e. the directory) which is to be deleted.
 * @return true If this path can be locked and the recursive delete started.
 * @return false If the delete cannot be started.
 */
char startRecursiveDelete( char* path );

/**
 * @brief Get a message containing the error of the last operation
 * @return ProtocolMessage_PathDeleted_t* 
 */
ProtocolMessage_PathDeleted_t *getDeleteError();

/**
 * @brief Get the next file which was deleted.
 * @return ProtocolMessage_PathDeleted_t* 
 */
ProtocolMessage_PathDeleted_t *getNextFileDeleted();

/**
 * @brief End the last recursive delete operation OR interrupt a currently running one.
 * 
 * @return true If there is a current recursive operation
 * @return false In all other cases
 */
char endRecursiveDelete();

/**
 * @brief Get a message to indicate the deletion of the message has been compelted.
 */
ProtocolMessage_PathDeleted_t *getRecusiveDeleteCompletedMessage();

/**
 * @brief Free any resources or memory allocated to the task of deletion.
 * 
 */
void cleanDeletePathGlobals();


#endif /* AMIGA_AMIGA_DELETEPATH_H_ */
