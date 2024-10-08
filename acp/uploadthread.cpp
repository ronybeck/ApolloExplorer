#include "uploadthread.h"
#include <QDebug>
#include <QtEndian>
#include <QApplication>
#include <QMessageBox>

#define DEBUG 0
#include "AEUtils.h"

#define LOCK QMutexLocker locker( &m_Mutex )
#define UNLOCK locker.unlock()
#define RELOCK locker.relock();

UploadThread::UploadThread(QObject *parent) :
    QThread(parent),
    m_Mutex( QMutex::Recursive ),
    m_ThroughPutTimer( nullptr ),
    m_UploadTimeoutTimer( nullptr ),
    m_ProtocolHandler( nullptr ),
    m_Connected( false ),
    m_FileToUpload( false ),
    m_JobType( JT_NONE ),
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
    m_ThroughPut( 0 ),
    m_InflightChunks(),
    m_MaxInflightChunks( 5 )
{

}

void UploadThread::run()
{
    LOCK;
    //Create our protocol handler
    m_ProtocolHandler = new ProtocolHandler();

    //Setup the throughput timer
    m_ThroughPutTimer = new QTimer();
    m_ThroughPutTimer->setTimerType( Qt::PreciseTimer );
    m_ThroughPutTimer->setInterval( 1000 );
    m_ThroughPutTimer->setSingleShot( false );
    m_ThroughPutTimer->start();
    connect( m_ThroughPutTimer, &QTimer::timeout, this, &UploadThread::onThroughputTimerExpiredSlot );

    //setup the upload timeout timer
    m_UploadTimeoutTimer = new QTimer();
    m_UploadTimeoutTimer->setTimerType( Qt::PreciseTimer );
    m_UploadTimeoutTimer->setInterval( 10000 );
    m_UploadTimeoutTimer->setSingleShot( true );
    connect( m_UploadTimeoutTimer, &QTimer::timeout, this, &UploadThread::onUploadTimeoutTimerExpiredSlot );
    connect( this, &UploadThread::stopUploadTimeoutTimerSignal, m_UploadTimeoutTimer, &QTimer::stop );
    connect( this, &UploadThread::startUploadTimeoutTimerSignal, m_UploadTimeoutTimer, QOverload<>::of( &QTimer::start ) );


    //Connect our protocol handler
    connect( m_ProtocolHandler, &ProtocolHandler::disconnectedFromHostSignal, this, &UploadThread::onDisconnectedFromHostSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::connectedToHostSignal, this, &UploadThread::onConnectedToHostSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::acknowledgeWithCodeSignal, this, &UploadThread::onAcknowledgeSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::outgoingByteCountSignal, this, &UploadThread::onOutgoingBytesUpdateSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::outgoingByteCountSignal, this, &UploadThread::outgoingBytesSignal );
    connect( m_ProtocolHandler, &ProtocolHandler::incomingByteCountSignal, this, &UploadThread::onIncomingBytesUpdateSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::fileChunkReceivedSignal, this, &UploadThread::onFileChunkReceivedSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::fileReceivedSignal, this, &UploadThread::onFilePutConfirmedSlot );
    connect( this, &UploadThread::sendMessageSignal, m_ProtocolHandler, &ProtocolHandler::onSendMessageSlot );
    connect( this, &UploadThread::sendAndReleaseMessageSignal, m_ProtocolHandler, &ProtocolHandler::onSendAndReleaseMessageSlot );
    connect( this, &UploadThread::connectToHostSignal, m_ProtocolHandler, &ProtocolHandler::onConnectToHostRequestedSlot );
    connect( this, &UploadThread::disconnectFromHostSignal, m_ProtocolHandler, &ProtocolHandler::onDisconnectFromHostRequestedSlot );
    connect( this, &UploadThread::createDirectorySignal, m_ProtocolHandler, &ProtocolHandler::onMKDirSlot );

    //Start the loop thread
    UNLOCK;
    m_Keeprunning = true;
    while( m_Keeprunning )
    {
        //Process events
        QApplication::processEvents();

        //Nothing to send yet?  Just loop around until there is
        RELOCK;
        if( !m_FileToUpload )
        {
            UNLOCK;
            QThread::yieldCurrentThread();
            QThread::msleep( 5 );
            continue;
        }
        UNLOCK;

        ProtocolMessage_FileChunk_t *nextFileChunk = AllocMessage< ProtocolMessage_FileChunk_t >();
        Q_ASSERT( nextFileChunk != nullptr );

        while( m_CurrentChunk < m_FileChunks && m_JobType == JT_UPLOAD )
        {
            QThread::yieldCurrentThread();
            QThread::msleep( 5 );

            RELOCK;
            //Check again that an interruption isn't requested
            if( this->isInterruptionRequested() )
            {
                UNLOCK;
                DBGLOG << "Stopping upload.";
                break;
            }

            //Are we even connected?
            if( !m_Connected )
            {
                UNLOCK;
                break;
            }

            //Read in the next file chunk
            if( !m_LocalFile.isOpen() )
            {
                UNLOCK;
                //The file has prematurely closed or some sort of error?
                DBGLOG << "The file was suddenly closed.";
                emit abortedSignal( "The local file was suddenly closed." );
                break;
            }

            //If there aren't already too many inflight file chunks, send another.
            if( m_InflightChunks.size() < static_cast<int>( m_MaxInflightChunks ) )
            {
                //Send the next file chunk
                m_InflightChunks.append( m_CurrentChunk );
                quint32 bytesRead = m_LocalFile.read( nextFileChunk->chunk, FILE_CHUNK_SIZE );
                quint32 messageSize = sizeof( *nextFileChunk );
                if( bytesRead < FILE_CHUNK_SIZE )
                {
                    messageSize -= FILE_CHUNK_SIZE - bytesRead;
                }
                //DBGLOG << "Chunk message size: " << messageSize;

                Q_ASSERT( messageSize > 0 );
                Q_ASSERT( messageSize < MAX_MESSAGE_LENGTH );

                nextFileChunk->header.token = MAGIC_TOKEN;
                nextFileChunk->header.type = PMT_FILE_CHUNK;
                nextFileChunk->header.length = messageSize;
                nextFileChunk->bytesContained = qToBigEndian<quint32>( bytesRead );
                nextFileChunk->chunkNumber = qToBigEndian<quint32>( m_CurrentChunk++ );
                m_ProtocolHandler->sendMessage( nextFileChunk );
                UNLOCK;

                //Calculate the statistics
                m_ProgressBytes += bytesRead;
                m_ProgressProcent = m_FileSize > 0 ? ( m_ProgressBytes * 100 / m_FileSize ) : 100;

                //Emit progress
                emit uploadProgressSignal( m_ProgressProcent, m_ProgressBytes, m_ThroughPut );
            }else
            {
                UNLOCK;
            }


            //Process events
            QApplication::processEvents();
        }

        //Cleanup
        ReleaseMessage( nextFileChunk );

        //Second check if there is a request to shut down the thread
        if( this->isInterruptionRequested() )
        {
            DBGLOG << "Shutting down upload thread.";
            break;
        }

        //Reset
        QThread::yieldCurrentThread();
        QThread::msleep( 5 );
    }

    //cleanup
    RELOCK;
    m_ThroughPutTimer->stop();
    delete m_ThroughPutTimer;
    delete m_UploadTimeoutTimer;
    delete m_ProtocolHandler;
    m_ThroughPutTimer = nullptr;
    m_UploadTimeoutTimer = nullptr;
    m_ProtocolHandler = nullptr;
}

void UploadThread::stopThread()
{
    m_Keeprunning = false;
}

void UploadThread::onCreateDirectorySlot( QString remotePath )
{
    LOCK;
    //Are we already doing something else?
    if( m_JobType == JT_UPLOAD )
        return;

    //Create the remote directory first
    m_AcknowledgeState = ProtocolHandler::AS_Unknown;
    m_JobType = JT_MKDIR;
    UNLOCK;

    //Create the directory
    emit createDirectorySignal( remotePath );

    //we are done
    return;
}

void UploadThread::onConnectToHostSlot(QHostAddress host, quint16 port)
{
    //Connect to the server
    DBGLOG << "Upload thread Connecting to server " << host.toString();
    //m_ProtocolHandler->onConnectToHostRequestedSlot( host, port );
    emit connectToHostSignal( host, port );
}

void UploadThread::onDisconnectFromHostRequestedSlot()
{
    LOCK;
    DBGLOG << "Upload thread disconnecting from server";
    emit disconnectFromHostSignal();
}


void UploadThread::onStartFileSlot( QString localFilePath, QString remoteFilePath )
{
    LOCK;

    DBGLOG << "Uploading file " << localFilePath << " to " << remoteFilePath;

    //Check if an upload is in progress
    if( m_FileToUpload )
    {
        DBGLOG << "Rejecting request to upload file " << localFilePath << " because a file upload is in progress.";
        return;
    }

    //Set the job type
    m_JobType = JT_UPLOAD_INIT;
    m_InflightChunks.clear();

    //Open the file in question
    m_LocalFilePath = localFilePath;
    m_RemoteFilePath = remoteFilePath;
    if( m_LocalFile.isOpen() )
    {
        //Close the file currently open
        m_LocalFile.close();
    }
    m_LocalFile.setFileName( m_LocalFilePath );
    m_LocalFile.open( QFile::ReadOnly );
    if( !m_LocalFile.isOpen() )
    {
        DBGLOG << "Unable to open local file: " << m_LocalFile.errorString();
        emit abortedSignal( m_LocalFile.errorString() );
        return;
    }


    //Setup the local variables for tracking
    m_FileSize = m_LocalFile.size();
    m_ProgressProcent = 0;
    m_ProgressBytes = 0;
    m_CurrentChunk = 0;

    //Special case.  It could be there is a file with a zero size.
    if( m_FileSize == 0 )
        m_FileChunks = 1;
    else
         m_FileChunks = m_FileSize / FILE_CHUNK_SIZE + ( m_FileSize % FILE_CHUNK_SIZE == 0 ? 0 : 1 );

    //Convert the remote path name to Amiga Text Encoding
    char encodedPath[ MAX_FILEPATH_LENGTH + 1 ];
    convertFromUTF8ToAmigaTextEncoding( m_RemoteFilePath, encodedPath, sizeof( encodedPath ) );

    //Form the request for the first file
    ProtocolMessage_FilePut_t *putFileRequest = AllocMessage<ProtocolMessage_FilePut_t>();
    putFileRequest->header.token = MAGIC_TOKEN;
    putFileRequest->header.type = PMT_PUT_FILE;
    putFileRequest->header.length = sizeof( ProtocolMessage_FilePull_t ) + strlen( encodedPath );
    strncpy( putFileRequest->filePath, encodedPath, strlen( encodedPath ) + 1 );
    putFileRequest->filePath[ strlen( encodedPath ) ] = 0;      //Make sure we have a terminating null

    //Send the message
    emit sendAndReleaseMessageSignal( reinterpret_cast<ProtocolMessage_t*>( putFileRequest ) );

    //Start the timer
    emit startUploadTimeoutTimerSignal();
}

void UploadThread::onCancelUploadSlot()
{
    LOCK;

    //To end the uploading of a file prematurely, we need to send an additional file chunk with zero bytes
    if( m_CurrentChunk < m_FileChunks )
    {
        ProtocolMessage_FileChunk_t *nextFileChunk = AllocMessage< ProtocolMessage_FileChunk_t >();
        Q_ASSERT( nextFileChunk != nullptr );

        nextFileChunk->header.token = MAGIC_TOKEN;
        nextFileChunk->header.type = PMT_FILE_CHUNK;
        nextFileChunk->header.length = sizeof( ProtocolMessage_FileChunk_t );
        nextFileChunk->bytesContained = 0;
        nextFileChunk->chunkNumber = 0;
        //m_ProtocolHandler->sendMessage( nextFileChunk );
        //ReleaseMessage( nextFileChunk );
        emit sendAndReleaseMessageSignal( reinterpret_cast<ProtocolMessage_t*>( nextFileChunk ) );
    }

    //Stop the timeout timer
    emit stopUploadTimeoutTimerSignal();

    //Now cleanup the upload book keeping
    cleanup();
}

void UploadThread::onFileChunkReceivedSlot(quint32 chunkNumber)
{
    LOCK;
    //DBGLOG << "Inflight chunk count: " << m_InflightChunks.count();
    if( m_InflightChunks.contains( chunkNumber ) )
    {
        m_InflightChunks.removeOne( chunkNumber );
    }
    emit startUploadTimeoutTimerSignal();  //Reset timer
}

void UploadThread::onFilePutConfirmedSlot(quint32 sizeWritten)
{
    //Stop the upload timeout timer
    emit stopUploadTimeoutTimerSignal();

    //Check that the uploaded file size was correct
    if( sizeWritten != m_FileSize )
    {
        DBGLOG << "File uploaded but the size is wrong!  Recieved size " << sizeWritten << " expected " << m_FileSize;
        cleanup();
        emit uploadFailedSignal( UF_SIZE_MISMATCH );
    }else
    {
        //Confirm the upload
        cleanup();
        emit uploadCompletedSignal();
    }
}

void UploadThread::onConnectedToHostSlot()
{
    m_Connected = true;
    DBGLOG << "Uploadthread connected to server.";
    emit connectedToServerSignal();
}

void UploadThread::onDisconnectedFromHostSlot()
{
    m_Connected = false;
    DBGLOG << "Uploadthread disconnected to server.";
    emit stopUploadTimeoutTimerSignal();
    emit disconnectedFromServerSignal();
}

void UploadThread::onIncomingBytesUpdateSlot(quint32 bytes)
{
    m_BytesReceivedThisSecond += bytes;
}

void UploadThread::onOutgoingBytesUpdateSlot(quint32 bytes)
{
    m_BytesSentThisSecond += bytes;
}

void UploadThread::onAcknowledgeSlot(quint8 responseCode)
{
    LOCK;

    //Interpret the response code
    QString errorMessage = "";
    switch( responseCode )
    {
        case AT_NOK:
            m_AcknowledgeState = ProtocolHandler::AS_FAILED;
            errorMessage="Unspecified error";
        break;
        case AT_OK:
            m_AcknowledgeState = ProtocolHandler::AS_SUCCESS;
        break;
        case AT_DEST_EXISTS_AS_FILE:
            m_AcknowledgeState = ProtocolHandler::AS_FAILED;
            errorMessage = "The destination already exists as a file";
        break;
        case AT_DEST_EXISTS_AS_DIR:
            m_AcknowledgeState = ProtocolHandler::AS_FAILED;
            errorMessage = "The destination already exists as a directory";
        break;
        default:
            m_AcknowledgeState = ProtocolHandler::AS_Unknown;
            errorMessage="Unspecified error";
        break;
    }

    //If we creating directories, we should signal this as being successful
    if( m_JobType == JT_MKDIR )
    {
        if( m_AcknowledgeState == ProtocolHandler::AS_SUCCESS ) emit directoryCreationCompletedSignal();
        else emit directoryCreationFailedSignal( errorMessage );
        return;
    }

    //If the server cancels the upload for some reason
    if( m_JobType == JT_UPLOAD || m_JobType == JT_UPLOAD_INIT || m_JobType == JT_UPLOAD_WAIT_ACK )
    {
        if( responseCode != AT_OK )
        {
            cleanup();
            DBGLOG << "File could not be uploaded.  Error code " << responseCode;
            emit abortedSignal( errorMessage );
            return;
        }
    }

    if( m_JobType == JT_UPLOAD_INIT )
    {
        //Send the start of file send request
        ProtocolMessage_StartOfFileSend_t *startOfFileSendMessage = AllocMessage<ProtocolMessage_StartOfFileSend_t>();
        startOfFileSendMessage->header.token = MAGIC_TOKEN;
        startOfFileSendMessage->header.type = PMT_START_OF_SEND_FILE;
        startOfFileSendMessage->header.length = sizeof( ProtocolMessage_StartOfFileSend_t ) + m_RemoteFilePath.length() + 1;
        strncpy( startOfFileSendMessage->filePath, m_RemoteFilePath.toStdString().c_str(), m_RemoteFilePath.length() + 1 );
        startOfFileSendMessage->fileSize = qToBigEndian<quint32>( m_FileSize );
        startOfFileSendMessage->numberOfFileChunks = qToBigEndian<quint32>( m_FileChunks );
        m_JobType = JT_UPLOAD_WAIT_ACK;
        m_FileToUpload = true;
        emit sendAndReleaseMessageSignal( reinterpret_cast<ProtocolMessage_t*>( startOfFileSendMessage ) );
        return;
    }

    if( m_JobType == JT_UPLOAD_WAIT_ACK )
    {
        //Now we are ready to start sending file chunks
        m_JobType = JT_UPLOAD;
    }
}

void UploadThread::onThroughputTimerExpiredSlot()
{
    m_ThroughPut = m_BytesSentThisSecond;
    m_BytesSentThisSecond = 0;
    //qDebug() << "Throughput: " << m_ThroughPut;
}

void UploadThread::onUploadTimeoutTimerExpiredSlot()
{
    DBGLOG << "Operation timed out";
    cleanup();
    emit operationTimedOutSignal();
}

void UploadThread::cleanup()
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
    m_FileToUpload = false;
    m_JobType= JT_NONE;
    m_InflightChunks.clear();
}
