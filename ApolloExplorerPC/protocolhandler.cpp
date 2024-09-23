#include "protocolhandler.h"
#include "messagepool.h"
#include <QtEndian>
#include <QEventLoop>

#define DEBUG 1
#include "AEUtils.h"

ProtocolHandler::ProtocolHandler(QObject *parent) :
    QObject(parent),
    m_AEConnection( this ),
    m_ServerAddress( ),
    m_ServerPort( 30202 ),
    m_ConnectionPhase( CP_DISCONNECTED )
{
    //setup signal slots
    connect( &m_AEConnection, &AEConnection::connectedToHostSignal, this, &ProtocolHandler::onConnectedSlot );
    connect( &m_AEConnection, &AEConnection::disconnectedFromHostSignal, this, &ProtocolHandler::onDisconnectedSlot );
    connect( &m_AEConnection, &AEConnection::newMessageReceived, this, &ProtocolHandler::onMessageReceivedSlot );
    connect( this, &ProtocolHandler::connectToHostSignal, &m_AEConnection, &AEConnection::onConnectToHostRequestedSlot );
    connect( this, &ProtocolHandler::disconnectFromHostSignal, &m_AEConnection, &AEConnection::onDisconnectFromhostRequestedSlot );

    //Through put signals
    connect( &m_AEConnection, &AEConnection::outgoingByteCountSignal, this, &ProtocolHandler::outgoingByteCountSignal );
    connect( &m_AEConnection, &AEConnection::incomingByteCountSignal, this, &ProtocolHandler::incomingByteCountSignal );

    //Raw Socket
    connect( this, &ProtocolHandler::rawOutgoingBytesSignal, &m_AEConnection, &AEConnection::onRawOutgoingBytesSlot );
    connect( &m_AEConnection, &AEConnection::rawIncomingBytesSignal, this, &ProtocolHandler::onRawIncomingBytesSlot );
}

void ProtocolHandler::onConnectToHostRequestedSlot( QHostAddress serverAddress, quint16 port )
{
    DBGLOG << "Connecting to host " << serverAddress << " on port " << port;
    //Save this for the reconnect later.
    m_ServerAddress = serverAddress;
    m_ServerPort = port;

    //We will now enter the initial connection phase
    m_ConnectionPhase = CP_CONNECTING;

    //Send this on to the connection
    emit connectToHostSignal( serverAddress, port );
}

void ProtocolHandler::onDisconnectFromHostRequestedSlot()
{
    emit disconnectFromHostSignal();
}

void ProtocolHandler::onSendMessageSlot( ProtocolMessage_t *message )
{
    m_AEConnection.onSendMessage( message );
}

void ProtocolHandler::onSendAndReleaseMessageSlot( ProtocolMessage_t *message )
{
    m_AEConnection.onSendMessage( message );
    ReleaseMessage( message );
}

void ProtocolHandler::onDeleteFileSlot(QString remotePath)
{
    //Form the file deletion message
    ProtocolMessage_DeletePath_t *deleteMessage = AllocMessage<ProtocolMessage_DeletePath_t>();
    deleteMessage->header.type = PMT_DELETE_PATH;
    deleteMessage->header.token = MAGIC_TOKEN;
    deleteMessage->header.length = sizeof( *deleteMessage ) + remotePath.size();
    deleteMessage->entryType = qToBigEndian( (unsigned int)DET_FILE );
    memset( deleteMessage->filePath, 0, MAX_FILEPATH_LENGTH );
    convertFromUTF8ToAmigaTextEncoding( remotePath, deleteMessage->filePath, remotePath.length() );

    //Send the message
    m_AEConnection.sendMessage( deleteMessage );
    ReleaseMessage( deleteMessage );
}

void ProtocolHandler::onDeleteRecursiveSlot(QString remotePath)
{
    //Form the file deletion message
    ProtocolMessage_DeletePath_t *deleteMessage = AllocMessage<ProtocolMessage_DeletePath_t>();
    deleteMessage->header.type = PMT_DELETE_PATH;
    deleteMessage->header.token = MAGIC_TOKEN;
    deleteMessage->header.length = sizeof( *deleteMessage ) + remotePath.size();
    deleteMessage->entryType = qToBigEndian( (unsigned int)DET_USERDIR );
    memset( deleteMessage->filePath, 0, MAX_FILEPATH_LENGTH );
    convertFromUTF8ToAmigaTextEncoding( remotePath, deleteMessage->filePath, remotePath.length() );

    //Send the message
    m_AEConnection.sendMessage( deleteMessage );
    ReleaseMessage( deleteMessage );
}

void ProtocolHandler::onCancelDeleteDirectorySlot()
{
    //TODO:
}

void ProtocolHandler::onGetVolumeListSlot()
{
    ProtocolMessage_t *getVolumeList = NewMessage();
    if( getVolumeList )
    {
            //Send the query
            getVolumeList->token = MAGIC_TOKEN;
            getVolumeList->type = PMT_GET_VOLUMES;
            getVolumeList->length = sizeof( ProtocolMessage_t );
            this->onSendMessageSlot( getVolumeList );
            FreeMessage( getVolumeList );
    }
}

void ProtocolHandler::onRunCMDSlot(QString command, QString workingDirectroy)
{
    Q_UNUSED( workingDirectroy )
    //Create the command message
    ProtocolMessage_Run_t *runMessage = AllocMessage<ProtocolMessage_Run_t>();
    runMessage->header.token = MAGIC_TOKEN;
    runMessage->header.type = PMT_RUN;
    runMessage->header.length = sizeof( ProtocolMessage_Run_t ) + command.length() + 1;
    strncpy( runMessage->command, command.toStdString().c_str(), command.length() + 1 );

    //Send the message
    m_AEConnection.sendMessage( runMessage );

    //Free Message
    ReleaseMessage( runMessage );

    //Enable raw mode
    //m_VNetConnection.onSetRawSocketMode();
}

void ProtocolHandler::onGetDirectorySlot( QString remoteDirectory )
{
    //Convert the text encoding
    char encodedPath[ MAX_FILEPATH_LENGTH ];
    convertFromUTF8ToAmigaTextEncoding( remoteDirectory, encodedPath, sizeof( encodedPath ) );

    ProtocolMessageGetDirectoryList_t *getDirMsg = AllocMessage<ProtocolMessageGetDirectoryList_t>();
    if( getDirMsg )
    {
        DBGLOG << "Getting dir path " << remoteDirectory;

        //Setup the message
        getDirMsg->length = strlen( encodedPath );
        memcpy( getDirMsg->path, encodedPath, getDirMsg->length );
        getDirMsg->path[ getDirMsg->length ] = 0;
        getDirMsg->header.token = MAGIC_TOKEN;
        getDirMsg->header.type = PMT_GET_DIR_LIST;
        getDirMsg->header.length = sizeof( ProtocolMessageGetDirectoryList_t ) + getDirMsg->length + 1;

        //Lastly, some endian conversion
        getDirMsg->length = qToBigEndian<unsigned int>( getDirMsg->length );

        //Send the message
        m_AEConnection.sendMessage( getDirMsg );

        //Free the message
        ReleaseMessage( getDirMsg );
    }
}

void ProtocolHandler::onMKDirSlot( QString remoteDirectory )
{
    //Convert the text encoding
    char encodedPath[ MAX_FILEPATH_LENGTH ];
    convertFromUTF8ToAmigaTextEncoding( remoteDirectory, encodedPath, sizeof( encodedPath ) );

    //Form the message
    ProtocolMessage_MakeDir_t *mkdirMessage = AllocMessage<ProtocolMessage_MakeDir_t>();
    mkdirMessage->header.token = MAGIC_TOKEN;
    mkdirMessage->header.type = PMT_MKDIR;
    mkdirMessage->header.length = sizeof( ProtocolMessage_MakeDir_t ) + remoteDirectory.length();
    strncpy( mkdirMessage->filePath, encodedPath, strlen( encodedPath ) + 1 );

    //Send the message
    m_AEConnection.sendMessage( mkdirMessage );
    ReleaseMessage( mkdirMessage );
}

void ProtocolHandler::onRenameFileSlot(QString oldPathName, QString newPathName)
{
    //Convert the text encoding
    char encodedOldName[ MAX_FILEPATH_LENGTH ];
    char encodedNewName[ MAX_FILEPATH_LENGTH ];
    convertFromUTF8ToAmigaTextEncoding( oldPathName, encodedOldName, sizeof( encodedOldName ) );
    convertFromUTF8ToAmigaTextEncoding( newPathName, encodedNewName, sizeof( encodedNewName ) );

    //Form the message
    ProtocolMessage_RenamePath_t *renameMessage = AllocMessage<ProtocolMessage_RenamePath_t>();
    renameMessage->header.token = MAGIC_TOKEN;
    renameMessage->header.type = PMT_RENAME_FILE;
    renameMessage->oldNameSize = oldPathName.length();
    renameMessage->newNameSize = newPathName.length();
    renameMessage->header.length = sizeof( ProtocolMessage_MakeDir_t ) + oldPathName.length() + newPathName.length() + 2;
    strncpy( renameMessage->filePaths, encodedOldName, strlen( encodedOldName ) + 1 );
    strncpy( renameMessage->filePaths + renameMessage->oldNameSize + 1, encodedNewName, strlen( encodedNewName ) + 1 );

    //NOw we should endian convert the sizes
    renameMessage->oldNameSize = qToBigEndian( renameMessage->oldNameSize );
    renameMessage->newNameSize = qToBigEndian( renameMessage->newNameSize );

    //Send the message
    m_AEConnection.sendMessage( renameMessage );
    ReleaseMessage( renameMessage );
}

bool ProtocolHandler::deleteFile( QString remoteFilePath, QString &error )
{
    //Convert the text encoding
    char encodedPath[ MAX_FILEPATH_LENGTH ];
    convertFromUTF8ToAmigaTextEncoding( remoteFilePath, encodedPath, sizeof( encodedPath ) );

    //Form the message
    ProtocolMessage_DeletePath_t *deletePathMessage = AllocMessage<ProtocolMessage_DeletePath_t>();
    deletePathMessage->header.token = MAGIC_TOKEN;
    deletePathMessage->header.type = PMT_DELETE_PATH;
    deletePathMessage->header.length = sizeof( ProtocolMessage_DeletePath_t ) + remoteFilePath.length();
    strncpy( deletePathMessage->filePath, encodedPath, strlen( encodedPath ) + 1 );

    //Send the message
    m_AEConnection.sendMessage( deletePathMessage );
    ReleaseMessage( deletePathMessage );

    //We want to wait for the OK or error message
    quint32 waitTime = 5000;
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    static bool succeeded = false;
    static bool deleteCompleted = false;
    connect( this, &ProtocolHandler::acknowledgeSignal, &loop, &QEventLoop::quit );
    connect( this, &ProtocolHandler::failedSignal, &loop, &QEventLoop::quit );
    connect( this, &ProtocolHandler::acknowledgeSignal, [&]()
            {
                succeeded = true;
                deleteCompleted = true;
            }
            );
    connect( this, &ProtocolHandler::failedSignal, [&]()
            {
                succeeded = false;
                deleteCompleted = true;
            }
            );
    connect( &timer, &QTimer::timeout, &loop, &QEventLoop::quit );
    timer.start( waitTime );

    //In case multiple parallel getDir() operations are going on, we should wait for the right one
    while( deleteCompleted == false )
    {
        loop.exec();
        if( !timer.isActive() )
            break;
    }

    //Did we timeout?
    if( !timer.isActive() )
    {
        error = "Operation Timed out";
        return false;
    }
    timer.stop();

    //Return our success or not
    return succeeded;
}

void ProtocolHandler::onConnectedSlot()
{
    if( m_ConnectionPhase == CP_RECONNECTING )
    {
        //Now we are connected
        m_ConnectionPhase = CP_CONNECTED;

        //We should ask for the version
        ProtocolMessage_t *versionQuery = AllocMessage<ProtocolMessage_t>();
        if( versionQuery )
        {
                //Send the query
                versionQuery->token = MAGIC_TOKEN;
                versionQuery->type = PMT_GET_VERSION;
                versionQuery->length = sizeof( ProtocolMessage_t );
                m_AEConnection.sendMessage( versionQuery );
                ReleaseMessage( versionQuery );
        }
    }

    //emit connectedToHostSignal();
}

void ProtocolHandler::onDisconnectedSlot()
{
    if( m_ConnectionPhase == CP_CONNECTED )
    {
        m_ConnectionPhase = CP_DISCONNECTED;
        emit disconnectedFromHostSignal();
    }
}

void ProtocolHandler::onMessageReceivedSlot( ProtocolMessage_t *newMessage )
{
    //Check which message we got
    switch( newMessage->type )
    {
        case PMT_NEW_CLIENT_PORT:
        {
            ProtocolMessageNewClientPort_t *newClientPort = reinterpret_cast<ProtocolMessageNewClientPort_t*>( newMessage );
            m_ServerPort = qFromBigEndian<quint16>( newClientPort->port );

            DBGLOG << "Server told us to connect to port " << m_ServerPort;

            //Now we are transitioning to the reconnect phase
            m_ConnectionPhase = CP_RECONNECTING;

            //start the reconnection
            m_AEConnection.onDisconnectFromhostRequestedSlot();
            m_AEConnection.onConnectToHostRequestedSlot( m_ServerAddress, m_ServerPort );
            break;
        }
        case PMT_VERSION:
        {
            //This signifies that we are now connected
            m_ConnectionPhase = CP_CONNECTED;

            ProtocolMessage_Version_t *versionMsg = reinterpret_cast<ProtocolMessage_Version_t*>( newMessage );
            DBGLOG << "Server version is: " << versionMsg->major << "." << versionMsg->minor << "." << versionMsg->rev;
            emit serverVersionSignal( versionMsg->major, versionMsg->minor, versionMsg->rev );
            emit connectedToHostSignal();
            break;
        }
        case PMT_DIR_LIST:
        {
            ProtocolMessageDirectoryList_t *dirListMsg = reinterpret_cast<ProtocolMessageDirectoryList_t*>( newMessage );

            //Create a new object for this
            DirectoryListing *newDirectoryListing = new DirectoryListing();
            newDirectoryListing->populate( dirListMsg );
            QSharedPointer<DirectoryListing> newDirectoryListingShrdPtr( newDirectoryListing );

            //Emit this.
            emit newDirectoryListingSignal( newDirectoryListingShrdPtr );
            break;
        }
        case PMT_ACK:
        {
            //Get the response out
            ProtocolMessage_Ack_t *ack = reinterpret_cast<ProtocolMessage_Ack_t*>( newMessage );
            quint8 response = ack->response;

            //emit this
            emit acknowledgeSignal();
            emit acknowledgeWithCodeSignal( response );
            break;
        }
        case PMT_FAILED:
        {
            ProtocolMessage_Failed_t *errorMsg = reinterpret_cast<ProtocolMessage_Failed_t*>( newMessage );
            QString message = convertFromAmigaTextEncoding( errorMsg->message );
            if( message.length() == 0 ){    message = "Unknown"; }
            emit failedWithReasonSignal( message );
            emit failedSignal();
            break;
        }
        case PMT_START_OF_SEND_FILE:
        {
            ProtocolMessage_StartOfFileSend_t *sofMsg = reinterpret_cast< ProtocolMessage_StartOfFileSend_t*>( newMessage );

            //Extract the details from the message
            QString filename = convertFromAmigaTextEncoding( sofMsg->filePath );
            quint64 fileSize = qFromBigEndian<quint32>( sofMsg->fileSize );
            quint32 numberOfChunks = qFromBigEndian<quint32>( sofMsg->numberOfFileChunks );

            //Emit this as a signal
            emit startOfFileSendSignal( fileSize, numberOfChunks, filename );
            break;
        }
        case PMT_FILE_CHUNK:
        {
            ProtocolMessage_FileChunk_t *fileChunkMsg = reinterpret_cast<ProtocolMessage_FileChunk_t*>( newMessage );

            //Extract the info out of the message
            quint32 chunkNumber = qFromBigEndian<quint32>( fileChunkMsg->chunkNumber );
            quint64 bytes = qFromBigEndian<quint32>( fileChunkMsg->bytesContained );
            QByteArray chunk( fileChunkMsg->chunk, bytes );

            //Emit the file chunk
            emit fileChunkSignal( chunkNumber, bytes, chunk );
            break;
        }
        case PMT_FILE_CHUNK_CONF:
        {
            ProtocolMessage_FileChunkConfirm_t *fileChunkConfMsg = reinterpret_cast<ProtocolMessage_FileChunkConfirm_t*>( newMessage );

            //Extract the info out of the message
            quint32 chunkNumber = qFromBigEndian<quint32>( fileChunkConfMsg->chunkNumber );

            //Emit the file chunk
            emit fileChunkReceivedSignal( chunkNumber );
            break;
        }
        case PMT_FILE_PUT_CONF:
        {
            ProtocolMessage_FilePutConfirm_t *filePutConfMsg = reinterpret_cast<ProtocolMessage_FilePutConfirm_t*>( newMessage );

            //Extract the info out of the message
            quint32 bytesWritten = qFromBigEndian<quint32>( filePutConfMsg->filesize );

            //Emit the file chunk
            emit fileReceivedSignal( bytesWritten );
            break;
        }
        case PMT_VOLUME_LIST:
        {
            ProtocolMessage_VolumeList_t *volumeListMessage = reinterpret_cast<ProtocolMessage_VolumeList_t*>( newMessage );
            QList<QSharedPointer<DiskVolume>> volumes;
            quint32 volumeCount = 0;

            //Endian conversion
            volumeListMessage->volumeCount = qFromBigEndian( volumeListMessage->volumeCount );

            //Go through the volume list
            VolumeEntry_t *volumeEntry = volumeListMessage->volumes;
            while( volumeCount < volumeListMessage->volumeCount )
            {
                //Get the name out
                QSharedPointer<DiskVolume> diskVolume( new DiskVolume( *volumeEntry ) );
                volumes.push_back( diskVolume );
                volumeCount++;

                //Get the next entry
                volumeEntry->entrySize = qFromBigEndian( volumeEntry->entrySize );
                volumeEntry = (VolumeEntry_t*)((char*)volumeEntry + volumeEntry->entrySize);
            }

            //Send out the result
            emit volumeListSignal( volumes );

            break;
        }
        case  PMT_PATH_DELETED:
        {
            ProtocolMessage_PathDeleted_t *pathDelMsg = reinterpret_cast<ProtocolMessage_PathDeleted_t*>( newMessage );
            QString path = convertFromAmigaTextEncoding( pathDelMsg->filePath );
            DBGLOG << "File " << path << " deleted " << ( pathDelMsg->deleteSucceeded == 1 ? "successfully" : "unsuccessfully" ) << ".";
            if( pathDelMsg->deleteSucceeded == 1 )
            {
                if( pathDelMsg->deleteCompleted == 1 )
                {
                    emit recursiveDeletionCompletedSignal();
                }else
                {
                    emit fileDeletedSignal( path );
                }
            }else
            {
                emit fileDeleteFailedSignal( path, pathDelMsg->failureReason );
            }
            break;
        }
        case PMT_PING:
        {
            //We can pretty much ignore this.  No pong required.......yet
            DBGLOG << "Got ping" ;
        }
        case PMT_CLOSING:
        {
            ProtocolMessageDisconnect_t *disconnectMessage = reinterpret_cast<ProtocolMessageDisconnect_t*>( newMessage );
            QString message( disconnectMessage->message );

            //Inform others what happened
            emit serverClosedConnectionSignal( message );
            break;
        }
        default:
            DBGLOG << "Unsupported message type " << newMessage->type;
        break;
    }
}

void ProtocolHandler::onRawOutgoingBytesSlot(QByteArray bytes)
{
    emit rawOutgoingBytesSignal( bytes );
}

void ProtocolHandler::onRawIncomingBytesSlot(QByteArray bytes)
{
    emit rawIncomingBytesSignal( bytes );
}
