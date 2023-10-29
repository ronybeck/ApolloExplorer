#include "iconthread.h"
#include <QDebug>
#include <QtEndian>
#include <QApplication>
#include <QMessageBox>

#define DEBUG 1
#include "AEUtils.h"


#define LOCK QMutexLocker locker( &m_Mutex )
#define UNLOCK locker.unlock()
#define RELOCK locker.relock();

IconThread::IconThread(QObject *parent) :
    QThread(parent),
    m_Mutex( QMutex::Recursive ),
    m_OperationTimer( nullptr ),
    m_ProtocolHandler( nullptr ),
    m_DirectoryListing(),
    m_OperationState( IDLE ),
    m_ConnectionState( DISCONNECTED ),
    m_RemoteFilePath( "" ),
    m_FileSize( 0 ),
    m_ProgressBytes( 0 ),
    m_ProgressProcent( 0 ),
    m_FileChunks( 0 ),
    m_CurrentChunk( 0 ),
    m_BytesSentThisSecond( 0 ),
    m_BytesReceivedThisSecond( 0 )
{
}

void IconThread::run()
{
    LOCK;
    //Create our protocol handler
    m_ProtocolHandler = new ProtocolHandler();

    //Connect our protocol handler
    connect( m_ProtocolHandler, &ProtocolHandler::disconnectedFromHostSignal, this, &IconThread::onDisconnectedFromHostSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::connectedToHostSignal, this, &IconThread::onConnectedToHostSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::acknowledgeWithCodeSignal, this, &IconThread::onAcknowledgeSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::startOfFileSendSignal, this, &IconThread::onStartOfFileSendSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::fileChunkSignal, this, &IconThread::onFileChunkSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::outgoingByteCountSignal, this, &IconThread::onOutgoingBytesUpdateSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::incomingByteCountSignal, this, &IconThread::onIncomingBytesUpdateSlot );
    connect( this, &IconThread::sendMessageSignal, m_ProtocolHandler, &ProtocolHandler::onSendMessageSlot );
    connect( this, &IconThread::sendAndReleaseMessageSignal, m_ProtocolHandler, &ProtocolHandler::onSendAndReleaseMessageSlot );
    connect( this, &IconThread::connectToHostSignal, m_ProtocolHandler, &ProtocolHandler::onConnectToHostRequestedSlot );
    connect( this, &IconThread::disconnectFromHostSignal, m_ProtocolHandler, &ProtocolHandler::onDisconnectFromHostRequestedSlot );


    //setup the operation timer
    m_OperationTimer = new QTimer();
    m_OperationTimer->setTimerType( Qt::PreciseTimer );
    m_OperationTimer->setSingleShot( true );
    m_OperationTimer->setInterval( 10000 );
    connect( this, &IconThread::startOperationTimerSignal, m_OperationTimer, QOverload<>::of( &QTimer::start ) );
    connect( this, &IconThread::stopOperationTimerSignal, m_OperationTimer, &QTimer::stop );
    connect( m_OperationTimer, &QTimer::timeout, this, &IconThread::onOperationTimerTimeoutSlot );


    //Start the loop thread
    UNLOCK;
    m_Keeprunning = true;
    while( m_Keeprunning )
    {
        //Process events
        QApplication::processEvents();

        //Second check if there is a request to shut down the thread
        if( this->isInterruptionRequested() )
        {
            qDebug() << "Shutting down upload thread.";
            break;
        }


        //Reset
        QThread::yieldCurrentThread();
        QThread::msleep( 10 );
    }

    //cleanup
    RELOCK;
    delete m_ProtocolHandler;
    m_ProtocolHandler = nullptr;
    delete m_OperationTimer;
}

void IconThread::stopThread()
{
    m_Keeprunning = false;
}

void IconThread::onConnectToHostSlot( QHostAddress host, quint16 port )
{
    //Connect to the server
    DBGLOG << "Icon thread Connecting to server " << host.toString();
    m_ConnectionState=CONNECTING;
    emit connectToHostSignal( host, port );
}

void IconThread::onDisconnectFromHostRequestedSlot()
{
    LOCK;
    DBGLOG << "Icon thread disconnecting from server";
    m_ConnectionState=DISCONNECTING;
    emit disconnectFromHostSignal();
}

void IconThread::onGetIconSlot(QString remoteFilePath)
{
    LOCK;

    m_RemoteFilePath = remoteFilePath;

    //Convert the remote path name to Amiga Text Encoding
    char encodedPath[ MAX_FILEPATH_LENGTH ];
    convertFromUTF8ToAmigaTextEncoding( m_RemoteFilePath, encodedPath, sizeof( encodedPath ) );

    //Form the request for the first file
    ProtocolMessage_FilePull_t *pullFileRequest = AllocMessage<ProtocolMessage_FilePull_t>();
    pullFileRequest->header.token = MAGIC_TOKEN;
    pullFileRequest->header.type = PMT_GET_FILE;
    pullFileRequest->header.length = sizeof( ProtocolMessage_FilePull_t ) + strlen( encodedPath );
    strncpy( pullFileRequest->filePath, encodedPath, strlen( encodedPath ) + 1 );
    pullFileRequest->filePath[ strlen( encodedPath ) ] = 0;      //Make sure we have a terminating null

    UNLOCK;

    //Start the operation timer
    emit startOperationTimerSignal();

    //Send the message
    emit sendAndReleaseMessageSignal( reinterpret_cast<ProtocolMessage_t*>( pullFileRequest ) );
}

void IconThread::onCancelDownloadSlot()
{
    LOCK;

    //Set the state to IDLE
    m_OperationState=IDLE;

    //Send the cancel operation message
    ProtocolMessage_CancelOperation_t *cancelMessage = AllocMessage<ProtocolMessage_CancelOperation_t>();
    cancelMessage->header.length = sizeof( *cancelMessage );
    cancelMessage->header.token = MAGIC_TOKEN;
    cancelMessage->header.type = PMT_CANCEL_OPERATION;

    emit sendAndReleaseMessageSignal( reinterpret_cast<ProtocolMessage_t*>( cancelMessage ) );

    //Now cleanup the upload book keeping
    cleanup();
}

void IconThread::onOperationTimerTimeoutSlot()
{
    DBGLOG << "Icon thread Operation timedout";
    if( m_OperationState != IDLE )
    {
        cleanup();
    }
    emit operationTimedOutSignal();
}

void IconThread::onConnectedToHostSlot()
{
    LOCK;
    DBGLOG << "Icon thread connected to server.";
    m_ConnectionState=CONNECTED;
    emit connectedToServerSignal();
}

void IconThread::onDisconnectedFromHostSlot()
{
    LOCK;
    DBGLOG << "Icon thread disconnected to server.";
    m_ConnectionState=DISCONNECTING;
    emit disconnectedFromServerSignal();
}

void IconThread::onIncomingBytesUpdateSlot(quint32 bytes)
{
    LOCK;
    m_BytesReceivedThisSecond += bytes;
}

void IconThread::onOutgoingBytesUpdateSlot(quint32 bytes)
{
    LOCK;
    m_BytesSentThisSecond += bytes;
}

void IconThread::onAcknowledgeSlot( quint8 responseCode )
{
    LOCK;

    //Stop the timer
    emit stopOperationTimerSignal();

    //DBGLOG << "Got an acknowledge from the server with a response of " << responseCode;
    //Check if our request has been accepted.
    if( responseCode == 0 )
    {
        DBGLOG << "The server said the file can't be downloaded for some reason.";
        cleanup();
        emit abortedSignal( "Rejected by server." );
        return;
    }

    //DBGLOG << "The server is allowing our file download.";
}

void IconThread::onStartOfFileSendSlot(quint64 fileSize, quint32 numberOfChunks, QString filename)
{
    Q_UNUSED( filename )
    //DBGLOG << "Got the start-of-download message for " << filename << "with " << numberOfChunks << " chunks and filesize " << fileSize;

    //Special case.  File to be downloaded if zero bytes in size
    if( fileSize == 0 && numberOfChunks == 0 )
    {
        DBGLOG << "File " << m_RemoteFilePath << " is zero bytes in size";
        cleanup();
        emit downloadCompletedSignal();
    }

    //Otherwise we are ok
    m_FileSize = fileSize;
    m_FileChunks = numberOfChunks;
    m_CurrentChunk = 0;

    //restart the operation timer
    emit startOperationTimerSignal();
}

void IconThread::onFileChunkSlot(quint32 chunkNumber, quint32 bytes, QByteArray chunk)
{
    LOCK;
    //If the number of bytes is empty, something went wrong
    if( bytes == 0 )
    {
        DBGLOG << "The server said the file can't be downloaded mid download.";
        emit stopOperationTimerSignal();
        emit abortedSignal( "Something went wrong on the server side." );
        cleanup();
        return;
    }

    //restart the operation timer
    emit startOperationTimerSignal();


    //Otherwise we need to write this to the disk
    m_CurrentChunk = chunkNumber;
    if( chunk.size() )
    {
        m_FileStorage.push_back( chunk );
    }

    //Update the progress
    m_ProgressBytes += bytes;
    if( m_FileSize > 0 )
    {
        m_ProgressProcent = m_ProgressBytes * 100 / m_FileSize;
    }else
    {
        m_ProgressProcent = 100;
    }

    //Are we done?
    if( m_CurrentChunk == ( m_FileChunks - 1 ) )
    {
        //Create an icon object
        QSharedPointer<AmigaInfoFile> newIcon( new AmigaInfoFile() );
        newIcon->process( m_FileStorage );

        //Send the icon off
        emit stopOperationTimerSignal();
        emit iconFileSignal( m_RemoteFilePath, newIcon );
        emit downloadCompletedSignal();

        //Now clean up and get ready for the next file
        cleanup();
    }
}


void IconThread::cleanup()
{
    LOCK;

    //Set the variables back to zero
    m_FileStorage.clear();
    m_FileSize = 0;
    m_ProgressBytes = 0;
    m_ProgressProcent = 0;
    m_FileChunks = 0;
    m_CurrentChunk = 0;
    m_OperationState=IDLE;
}
