#include "uploadthread.h"
#include <QDebug>
#include <QtEndian>
#include <QApplication>
#include <QMessageBox>

#define DEBUG 1
#include "AEUtils.h"

#define LOCK QMutexLocker locker( &m_Mutex )
#define UNLOCK locker.unlock()
#define RELOCK locker.relock();

UploadThread::UploadThread(QObject *parent) :
    QThread(parent),
    m_Mutex( QMutex::Recursive ),
    m_ThroughPutTimer( this ),
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
    m_ThroughPut( 0 )
{
    //Setup the throughput timer
    m_ThroughPutTimer.setInterval( 1000 );
    m_ThroughPutTimer.setSingleShot( false );
    m_ThroughPutTimer.start();

    connect( &m_ThroughPutTimer, &QTimer::timeout, this, &UploadThread::onThroughputTimerExpiredSlot );
}

void UploadThread::run()
{
    LOCK;
    //Create our protocol handler
    m_ProtocolHandler = new ProtocolHandler();

    //Connect our protocol handler
    connect( m_ProtocolHandler, &ProtocolHandler::disconnectFromHostSignal, this, &UploadThread::onDisconnectedFromHostSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::connectToHostSignal, this, &UploadThread::onConnectedToHostSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::acknowledgeWithCodeSignal, this, &UploadThread::onAcknowledgeSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::outgoingByteCountSignal, this, &UploadThread::onOutgoingBytesUpdateSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::outgoingByteCountSignal, this, &UploadThread::outgoingBytesSignal );
    connect( m_ProtocolHandler, &ProtocolHandler::incomingByteCountSignal, this, &UploadThread::onIncomingBytesUpdateSlot );
    connect( this, &UploadThread::sendMessageSignal, m_ProtocolHandler, &ProtocolHandler::onSendMessageSlot );
    connect( this, &UploadThread::sendAndReleaseMessageSignal, m_ProtocolHandler, &ProtocolHandler::onSendAndReleaseMessageSlot );
    connect( this, &UploadThread::connectToHostSignal, m_ProtocolHandler, &ProtocolHandler::onConnectToHostRequestedSlot );
    connect( this, &UploadThread::disconnectFromHostSignal, m_ProtocolHandler, &ProtocolHandler::onDisconnectFromHostRequestedSlot );
    connect( this, &UploadThread::createDirectorySignal, m_ProtocolHandler, &ProtocolHandler::onMKDirSlot );

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

        //Nothing to send yet?  Just loop around until there is
        RELOCK;
        if( !m_FileToUpload )
        {
            UNLOCK;
            QThread::yieldCurrentThread();
            QThread::msleep( 100 );
            continue;
        }
        UNLOCK;

        ProtocolMessage_FileChunk_t *nextFileChunk = AllocMessage< ProtocolMessage_FileChunk_t >();
        Q_ASSERT( nextFileChunk != nullptr );

        while( m_CurrentChunk < m_FileChunks )
        {
            RELOCK;

            //Check again that an interruption isn't requested
            if( this->isInterruptionRequested() )
            {
                UNLOCK;
                qDebug() << "Stopping upload.";
                break;
            }

            //Are we even connected?
            if( !m_Connected )
            {
                UNLOCK;
                QThread::yieldCurrentThread();
                QThread::msleep( 10 );
                continue;
            }

            //Read in the next file chunk
            if( !m_LocalFile.isOpen() )
            {
                UNLOCK;
                //The file has prematurely closed or some sort of error?
                qDebug() << "UploadThread: The file was suddenly closed.";
                emit abortedSignal( "The local file was suddenly closed." );
                break;;
            }

            //Setup the next chunk file
            quint32 bytesRead = m_LocalFile.read( nextFileChunk->chunk, FILE_CHUNK_SIZE );
            nextFileChunk->header.token = MAGIC_TOKEN;
            nextFileChunk->header.type = PMT_FILE_CHUNK;
            nextFileChunk->header.length = sizeof( ProtocolMessage_FileChunk_t );
            nextFileChunk->bytesContained = qToBigEndian<quint32>( bytesRead );
            nextFileChunk->chunkNumber = qToBigEndian<quint32>( m_CurrentChunk++ );
            m_ProtocolHandler->sendMessage( nextFileChunk );

            //Process events
            QApplication::processEvents();

            //Calculate the statistics
            m_ProgressBytes += bytesRead;
            m_ProgressProcent = m_FileSize > 0 ? ( m_ProgressBytes * 100 / m_FileSize ) : 100;

            //Emit progress
            emit uploadProgressSignal( m_ProgressProcent, m_ProgressBytes, m_ThroughPut );

            UNLOCK;
            QThread::yieldCurrentThread();
            QThread::msleep( 10 );
        }

        //Signal completion
        qDebug() << "UploadThread: Upload completed.";
        emit uploadCompletedSignal();


        //Cleanup
        ReleaseMessage( nextFileChunk );
        cleanup();

        //Reset
        QThread::yieldCurrentThread();
        QThread::msleep( 10 );
    }

    //cleanup
    RELOCK;
    delete m_ProtocolHandler;
    m_ProtocolHandler = nullptr;
}

bool UploadThread::createDirectory( QString remotePath )
{
    //Create the remote directory first
    m_AcknowledgeState = ProtocolHandler::AS_Unknown;

    m_JobType = JT_MKDIR;

    //Wait on the reply with a timeout
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    connect( m_ProtocolHandler, &ProtocolHandler::acknowledgeSignal, &loop, &QEventLoop::quit );
    connect( &timer, &QTimer::timeout, &loop, &QEventLoop::quit );
    timer.start( 10000 );    //Wait up to 10 seconds.  Note:  Floppies and CDROMS are SLLLLLOOOOOOWWWWW
    emit createDirectorySignal( remotePath );   //Send the command now
    loop.exec();

    //Did we timeout?  Did the dir creation succeed?
    if( !timer.isActive() || m_AcknowledgeState != ProtocolHandler::AS_SUCCESS )
    {
        qDebug() << "Timeout exceeded creating directory " << remotePath;
        QMessageBox errorBox( QMessageBox::Critical, "Timeout creating remote directory", "A timeout occurred while creating remote directory" + remotePath, QMessageBox::Ok );
        errorBox.exec();
        return false;
    }

    //we are done
    return true;
}

void UploadThread::onConnectToHostSlot(QHostAddress host, quint16 port)
{
    //Connect to the server
    qDebug() << "Upload thread Connecting to server " << host.toString();
    //m_ProtocolHandler->onConnectToHostRequestedSlot( host, port );
    emit connectToHostSignal( host, port );
}

void UploadThread::onDisconnectFromHostRequestedSlot()
{
    LOCK;
    qDebug() << "Upload thread disconnecting from server";
    emit disconnectFromHostSignal();
}


void UploadThread::onStartFileSlot( QString localFilePath, QString remoteFilePath )
{
    LOCK;

    DBGLOG << "Uploading file " << localFilePath << " to " << remoteFilePath;

    //Check if an upload is in progress
    if( m_FileToUpload )
    {
        qDebug() << "Rejecting request to upload file " << localFilePath << " because a file upload is in progress.";
        return;
    }

    //Set the job type
    m_JobType = JT_UPLOAD;

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
        qDebug() << "Unable to open local file: " << m_LocalFile.errorString();
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

    //Now cleanup the upload book keeping
    cleanup();
}

void UploadThread::onConnectedToHostSlot()
{
    LOCK;
    m_Connected = true;
    qDebug() << "Uploadthread connected to server.";
    emit connectedToServerSignal();
}

void UploadThread::onDisconnectedFromHostSlot()
{
    LOCK;
    m_Connected = false;
    qDebug() << "Uploadthread disconnected to server.";
    emit disconnectedFromServerSignal();
}

void UploadThread::onIncomingBytesUpdateSlot(quint32 bytes)
{
    LOCK;
    m_BytesReceivedThisSecond += bytes;
}

void UploadThread::onOutgoingBytesUpdateSlot(quint32 bytes)
{
    LOCK;
    m_BytesSentThisSecond += bytes;
}

void UploadThread::onAcknowledgeSlot(quint8 responseCode)
{
    LOCK;

    //Interpret the response code
    switch( responseCode )
    {
        case 0:
            m_AcknowledgeState = ProtocolHandler::AS_FAILED;
        break;
        case 1:
            m_AcknowledgeState = ProtocolHandler::AS_SUCCESS;
        break;
        default:
            m_AcknowledgeState = ProtocolHandler::AS_Unknown;
        break;
    }

    //If we are not uploading a file, there is nothing more to do
    if( m_JobType != JT_UPLOAD ) return;

    if( responseCode == 0 )
    {
        //TODO: Should this set m_FileToUpload to false?
        qDebug() << "File could not be uploaded.  Error code " << responseCode;
        emit abortedSignal( "Upload rejected by server." );
        return;
    }

    //Ignore possitive acknowledtements if we are already uploading
    if( m_FileToUpload && responseCode == 1 )
        return;

    //Send the start of file send request
    ProtocolMessage_StartOfFileSend_t *startOfFileSendMessage = AllocMessage<ProtocolMessage_StartOfFileSend_t>();
    startOfFileSendMessage->header.token = MAGIC_TOKEN;
    startOfFileSendMessage->header.type = PMT_START_OF_SEND_FILE;
    startOfFileSendMessage->header.length = sizeof( ProtocolMessage_StartOfFileSend_t ) + m_RemoteFilePath.length() + 1;
    strncpy( startOfFileSendMessage->filePath, m_RemoteFilePath.toStdString().c_str(), m_RemoteFilePath.length() + 1 );
    startOfFileSendMessage->fileSize = qToBigEndian<quint32>( m_FileSize );
    startOfFileSendMessage->numberOfFileChunks = qToBigEndian<quint32>( m_FileChunks );
    //m_ProtocolHandler->sendMessage( startOfFileSendMessage );
    //ReleaseMessage( startOfFileSendMessage );
    emit sendAndReleaseMessageSignal( reinterpret_cast<ProtocolMessage_t*>( startOfFileSendMessage ) );

    m_FileToUpload = true;
}

void UploadThread::onThroughputTimerExpiredSlot()
{
    LOCK;
    m_ThroughPut = m_BytesSentThisSecond;
    m_BytesSentThisSecond = 0;
    //qDebug() << "Throughput: " << m_ThroughPut;
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
}
