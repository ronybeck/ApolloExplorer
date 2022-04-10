#include "protocolhandler.h"
#include "messagepool.h"
#include <QtEndian>
#include <QEventLoop>

#include "VNetPCUtils.h"

ProtocolHandler::ProtocolHandler(QObject *parent) :
    QObject(parent),
    m_VNetConnection( this ),
    m_ServerAddress( ),
    m_ServerPort( 30202 ),
    m_ConnectionPhase( CP_DISCONNECTED )
{
    //setup signal slots
    connect( &m_VNetConnection, &VNetConnection::connectedToHostSignal, this, &ProtocolHandler::onConnectedSlot );
    connect( &m_VNetConnection, &VNetConnection::disconnectedFromHostSignal, this, &ProtocolHandler::onDisconnectedSlot );
    connect( &m_VNetConnection, &VNetConnection::newMessageReceived, this, &ProtocolHandler::onMessageReceivedSlot );
    connect( this, &ProtocolHandler::connectToHostSignal, &m_VNetConnection, &VNetConnection::onConnectToHostRequestedSlot );
    connect( this, &ProtocolHandler::disconnectFromHostSignal, &m_VNetConnection, &VNetConnection::onDisconnectFromhostRequestedSlot );

    //Through put signals
    connect( &m_VNetConnection, &VNetConnection::outgoingByteCountSignal, this, &ProtocolHandler::outgoingByteCountSignal );
    connect( &m_VNetConnection, &VNetConnection::incomingByteCountSignal, this, &ProtocolHandler::incomingByteCountSignal );

    //Raw Socket
    connect( this, &ProtocolHandler::rawOutgoingBytesSignal, &m_VNetConnection, &VNetConnection::onRawOutgoingBytesSlot );
    connect( &m_VNetConnection, &VNetConnection::rawIncomingBytesSignal, this, &ProtocolHandler::onRawIncomingBytesSlot );
}

void ProtocolHandler::onConnectToHostRequestedSlot( QHostAddress serverAddress, quint16 port )
{
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
    m_VNetConnection.onSendMessage( message );
}

void ProtocolHandler::onSendAndReleaseMessageSlot( ProtocolMessage_t *message )
{
    m_VNetConnection.onSendMessage( message );
    ReleaseMessage( message );
}

void ProtocolHandler::onRunCMDSlot(QString command, QString workingDirectroy)
{
    //Create the command message
    ProtocolMessage_Run_t *runMessage = AllocMessage<ProtocolMessage_Run_t>();
    runMessage->header.token = MAGIC_TOKEN;
    runMessage->header.type = PMT_RUN;
    runMessage->header.length = sizeof( ProtocolMessage_Run_t ) + command.length() + 1;
    strncpy( runMessage->command, command.toStdString().c_str(), command.length() + 1 );

    //Send the message
    m_VNetConnection.sendMessage( runMessage );

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
        qDebug() << "Getting dir path " << remoteDirectory;

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
        m_VNetConnection.sendMessage( getDirMsg );

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
    m_VNetConnection.sendMessage( mkdirMessage );
    ReleaseMessage( mkdirMessage );
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
    m_VNetConnection.sendMessage( deletePathMessage );
    ReleaseMessage( deletePathMessage );

    //We want to wait for the OK or error message
    quint32 waitTime = 1000;
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    static bool succeeded = false;
    static bool deleteCompleted = false;
    connect( this, &ProtocolHandler::acknowledgeSignal, &loop, &QEventLoop::quit );
    connect( this, &ProtocolHandler::failedSignal, &loop, &QEventLoop::quit );
    connect( this, &ProtocolHandler::acknowledgeSignal, [&](){ succeeded = true; deleteCompleted = true; } );
    connect( this, &ProtocolHandler::failedSignal, [&]{ succeeded = false; deleteCompleted = true; } );
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
        error = "Operation Timedout";
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
                m_VNetConnection.sendMessage( versionQuery );
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

            qDebug() << "Server told us to connect to port " << m_ServerPort;

            //Now we are transitioning to the reconnect phase
            m_ConnectionPhase = CP_RECONNECTING;

            //start the reconnection
            m_VNetConnection.onDisconnectFromhostRequestedSlot();
            m_VNetConnection.onConnectToHostRequestedSlot( m_ServerAddress, m_ServerPort );
            break;
        }
        case PMT_VERSION:
        {
            //This signifies that we are now connected
            m_ConnectionPhase = CP_CONNECTED;

            ProtocolMessage_Version_t *versionMsg = reinterpret_cast<ProtocolMessage_Version_t*>( newMessage );
            qDebug().nospace().noquote() << "Server version is: " << versionMsg->major << "." << versionMsg->minor << "." << versionMsg->rev;
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
        case PMT_VOLUME_LIST:
        {
            ProtocolMessage_VolumeList_t *volumeListMessage = reinterpret_cast<ProtocolMessage_VolumeList_t*>( newMessage );
            QStringList volumes;
            quint32 volumeCount = 0;

            //Endian conversion
            volumeListMessage->volumeCount = qFromBigEndian( volumeListMessage->volumeCount );

            //Go through the volume list
            VolumeEntry_t *volumeEntry = volumeListMessage->volumes;
            while( volumeCount < volumeListMessage->volumeCount )
            {
                //Get the name out
                QString volume( static_cast<char*>( volumeEntry->name ) );
                if( volume.length() ) volumes.push_back( volume );
                volumeCount++;

                //Get the next entry
                volumeEntry->entrySize = qFromBigEndian( volumeEntry->entrySize );
                volumeEntry = (VolumeEntry_t*)((char*)volumeEntry + volumeEntry->entrySize);
            }

            //Send out the result
            emit volumeListSignal( volumes );

            break;
        }
        default:
            qDebug() << "Unsupported message type " << newMessage->type;
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
