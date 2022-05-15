#include "downloadthread.h"
#include <QDebug>
#include <QtEndian>
#include <QApplication>
#include <QMessageBox>

#define DEBUG 0
#include "AEUtils.h"


#define LOCK QMutexLocker locker( &m_Mutex )
#define UNLOCK locker.unlock()
#define RELOCK locker.relock();

DownloadThread::DownloadThread(QObject *parent) :
    QThread(parent),
    m_Mutex( QMutex::Recursive ),
    m_ThroughPutTimer( this ),
    m_ProtocolHandler( nullptr ),
    m_Connected( false ),
    m_FileToDownload( false ),
    m_RemoteFilePath( "" ),
    m_LocalFilePath( "" ),
    m_LocalFile(),
    m_FileSize( 0 ),
    m_ProgressBytes( 0 ),
    m_ProgressProcent( 0 ),
    m_FileChunks( 0 ),
    m_CurrentChunk( 0 ),
    m_BytesSentThisSecond( 0 ),
    m_BytesReceivedThisSecond( 0 ),
    m_ThroughPut( 0 )
{
    //Setup the throughput timer
    m_ThroughPutTimer.setInterval( 1000 );
    m_ThroughPutTimer.setSingleShot( false );
    m_ThroughPutTimer.start();

    connect( &m_ThroughPutTimer, &QTimer::timeout, this, &DownloadThread::onThroughputTimerExpiredSlot );
}

void DownloadThread::run()
{
    LOCK;
    //Create our protocol handler
    m_ProtocolHandler = new ProtocolHandler();

    //Connect our protocol handler
    connect( m_ProtocolHandler, &ProtocolHandler::disconnectFromHostSignal, this, &DownloadThread::onDisconnectedFromHostSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::connectToHostSignal, this, &DownloadThread::onConnectedToHostSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::acknowledgeWithCodeSignal, this, &DownloadThread::onAcknowledgeSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::startOfFileSendSignal, this, &DownloadThread::onStartOfFileSendSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::fileChunkSignal, this, &DownloadThread::onFileChunkSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::outgoingByteCountSignal, this, &DownloadThread::onOutgoingBytesUpdateSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::incomingByteCountSignal, this, &DownloadThread::onIncomingBytesUpdateSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::newDirectoryListingSignal, this, &DownloadThread::onDirectoryListingSlot );
    connect( this, &DownloadThread::sendMessageSignal, m_ProtocolHandler, &ProtocolHandler::onSendMessageSlot );
    connect( this, &DownloadThread::sendAndReleaseMessageSignal, m_ProtocolHandler, &ProtocolHandler::onSendAndReleaseMessageSlot );
    connect( this, &DownloadThread::connectToHostSignal, m_ProtocolHandler, &ProtocolHandler::onConnectToHostRequestedSlot );
    connect( this, &DownloadThread::disconnectFromHostSignal, m_ProtocolHandler, &ProtocolHandler::onDisconnectFromHostRequestedSlot );
    connect( this, &DownloadThread::getRemoteDirectorySignal, m_ProtocolHandler, &ProtocolHandler::onGetDirectorySlot );

    //Start the loop thread
    UNLOCK;
    while( 1 )
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
}

void DownloadThread::onConnectToHostSlot( QHostAddress host, quint16 port )
{
    //Connect to the server
    DBGLOG << "Download thread Connecting to server " << host.toString();
    emit connectToHostSignal( host, port );
}

void DownloadThread::onDisconnectFromHostRequestedSlot()
{
    LOCK;
    DBGLOG << "Download thread disconnecting from server";
    emit disconnectFromHostSignal();
}

void DownloadThread::onStartFileSlot(QString localFilePath, QString remoteFilePath)
{
    m_LocalFilePath = localFilePath;
    m_RemoteFilePath = remoteFilePath;

    DBGLOG << "Opening local file " << m_LocalFilePath;
    m_LocalFile.setFileName( m_LocalFilePath );
    m_LocalFile.open( QFile::ReadWrite );
    if( !m_LocalFile.isWritable() || !m_LocalFile.isOpen() )
    {
        QString errorMessage = m_LocalFile.errorString();
        cleanup();
        emit abortedSignal( errorMessage );
        return;
    }

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

    //Send the message
    emit sendAndReleaseMessageSignal( reinterpret_cast<ProtocolMessage_t*>( pullFileRequest ) );
}

void DownloadThread::onCancelDownloadSlot()
{
    LOCK;

    //Send the cancel operation message
    ProtocolMessage_CancelOperation_t *cancelMessage = AllocMessage<ProtocolMessage_CancelOperation_t>();
    cancelMessage->header.length = sizeof( *cancelMessage );
    cancelMessage->header.token = MAGIC_TOKEN;
    cancelMessage->header.type = PMT_CANCEL_OPERATION;

    emit sendAndReleaseMessageSignal( reinterpret_cast<ProtocolMessage_t*>( cancelMessage ) );

    //Now cleanup the upload book keeping
    cleanup();
}

QSharedPointer<DirectoryListing> DownloadThread::onGetDirectoryListingSlot( QString remotePath )
{
    DBGLOG << "Getting remote directory " << remotePath;

    //Now get a list of the remote files
    quint32 waitTime = 60000;  //Wait up to 120 seconds.  Note:  This is not over doing it.  I have seen WB take 60 seconds to get a directory list of BeneathASteelSky for the CD32.  We will take longer.
    //Wait on the reply with a timeout
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    connect( this, &DownloadThread::directoryListingReceivedSignal, &loop, &QEventLoop::quit );
    connect( &timer, &QTimer::timeout, &loop, &QEventLoop::quit );
    timer.start( waitTime );
    emit getRemoteDirectorySignal( remotePath );
    loop.exec();

    //Did a timeout occur?
    if( !timer.isActive() )
    {
        DBGLOG << "Time out occured getting remote path " << remotePath;
        return nullptr;
    }

    //So we got the path it seems.  Give it back to the caller
    return m_DirectoryListing;
}

void DownloadThread::onConnectedToHostSlot()
{
    LOCK;
    m_Connected = true;
    DBGLOG << "Download thread connected to server.";
    emit connectedToServerSignal();
}

void DownloadThread::onDisconnectedFromHostSlot()
{
    LOCK;
    m_Connected = false;
    DBGLOG << "Download thread disconnected to server.";
    emit disconnectedFromServerSignal();
}

void DownloadThread::onIncomingBytesUpdateSlot(quint32 bytes)
{
    LOCK;
    m_BytesReceivedThisSecond += bytes;
}

void DownloadThread::onOutgoingBytesUpdateSlot(quint32 bytes)
{
    LOCK;
    m_BytesSentThisSecond += bytes;
}

void DownloadThread::onAcknowledgeSlot( quint8 responseCode )
{
    LOCK;

    DBGLOG << "Got an acknowledge from the server with a response of " << responseCode;
    //Check if our request has been accepted.
    if( responseCode == 0 )
    {
        DBGLOG << "The server said the file can't be downloaded for some reason.";
        cleanup();
        emit abortedSignal( "Rejected by server." );
        return;
    }

    DBGLOG << "The server is allowing our file download.";
}

void DownloadThread::onStartOfFileSendSlot(quint64 fileSize, quint32 numberOfChunks, QString filename)
{
    DBGLOG << "Got the start-of-download message for " << filename << "with " << numberOfChunks << " chunks and filesize " << fileSize;

    //Special case.  File to be downloaded if zero bytes in size
    if( fileSize == 0 && numberOfChunks == 0 )
    {
        DBGLOG << "File " << m_LocalFile.fileName() << " is zero bytes in size";
        cleanup();
        emit downloadCompletedSignal();
    }

    //Otherwise we are ok
    m_FileSize = fileSize;
    m_FileChunks = numberOfChunks;
    m_CurrentChunk = 0;
}

void DownloadThread::onFileChunkSlot(quint32 chunkNumber, quint32 bytes, QByteArray chunk)
{
    LOCK;
    //If the number of bytes is empty, something went wrong
    if( bytes == 0 )
    {
        DBGLOG << "The server said the file can't be downloaded mid download.";
        emit abortedSignal( "Something went wrong on the server side." );
        cleanup();
        return;
    }


    //Otherwise we need to write this to the disk

    m_CurrentChunk = chunkNumber;
    if( chunk.size() )
    {
        m_LocalFile.write( chunk );
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

    //Emit the progress
    emit downloadProgressSignal( m_ProgressProcent, m_ProgressBytes, m_ThroughPut );

    //Are we done?
    if( m_CurrentChunk == ( m_FileChunks - 1 ) )
    {
        cleanup();
        emit downloadCompletedSignal();
    }
}

void DownloadThread::onThroughputTimerExpiredSlot()
{
    LOCK;
    m_ThroughPut = m_BytesReceivedThisSecond;
    m_BytesReceivedThisSecond = 0;
}

void DownloadThread::onDirectoryListingSlot(QSharedPointer<DirectoryListing> listing)
{
    m_DirectoryListing = listing;
    emit directoryListingReceivedSignal();
}

void DownloadThread::cleanup()
{
    LOCK;

    //Set the variables back to zero
    m_RemoteFilePath = "";
    m_LocalFilePath = "";
    if( m_LocalFile.isOpen() )
    {
        m_LocalFile.close();
    }
    m_FileSize = 0;
    m_ProgressBytes = 0;
    m_ProgressProcent = 0;
    m_FileChunks = 0;
    m_CurrentChunk = 0;
    m_FileToDownload = false;
}
