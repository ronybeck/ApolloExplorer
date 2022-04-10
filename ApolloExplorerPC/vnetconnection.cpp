#include "vnetconnection.h"

#include <QDebug>
#include <QtEndian>
#include <QApplication>
#include <QThread>

#define INPUT_BUFFER_SIZE 0x20000

VNetConnection::VNetConnection(QObject *parent) :
    QObject(parent),
    m_Socket(),
    m_RawSocketModeActive( false ),
    m_IncomingMessageBuffer( nullptr ),
    m_TotalBytesRead( 0 ),
    m_TotalBytesLeftToRead( 0 ),
    m_OutgoingByteCount( 0 ),
    m_IncomgingByteCount( 0 )

{
    //Signal/Slots
    connect( &m_Socket, &QTcpSocket::connected, this, &VNetConnection::onConnectedSlot );
    connect( &m_Socket, &QTcpSocket::disconnected, this, &VNetConnection::onDisconnectedSlot );
    connect( &m_Socket, &QTcpSocket::readyRead, this, &VNetConnection::onReadReadySlot );
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    connect( &m_Socket, &QTcpSocket::errorOccurred, this, &VNetConnection::onErrorSlot );
#else
    connect( &m_Socket, static_cast<void (QTcpSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error), this, &VNetConnection::onErrorSlot );
#endif

    //Set a message buffer
    m_IncomingMessageBuffer = static_cast<ProtocolMessage_t*>( calloc( 1, INPUT_BUFFER_SIZE ) );

    //Setup the throughput timer
    connect( &m_ThroughputTimer, &QTimer::timeout, this, &VNetConnection::onThroughputTimerExpiredSlot );
    m_ThroughputTimer.setInterval( 1000 );
    m_ThroughputTimer.setSingleShot( false );
    m_ThroughputTimer.start();

    //m_Socket.setSocketOption( QAbstractSocket::SendBufferSizeSocketOption, 0 );
    //m_Socket.setSocketOption( QAbstractSocket::SendBufferSizeSocketOption, quint64( FILE_CHUNK_SIZE + sizeof( ProtocolMessage_t ) ) );
}

VNetConnection::~VNetConnection()
{
    if( m_IncomingMessageBuffer ) free( m_IncomingMessageBuffer );
}

void VNetConnection::onConnectToHostRequestedSlot(QHostAddress serverAddress, quint16 port )
{
    //Connect to the host
    m_Socket.connectToHost( serverAddress, port );

    qDebug() << "Connecting to server " << serverAddress << " on port " << port;
}

void VNetConnection::onDisconnectFromhostRequestedSlot()
{
    m_Socket.disconnectFromHost();
    if( m_Socket.state() != QAbstractSocket::UnconnectedState )
    {
        m_Socket.waitForDisconnected();
    }
}

void VNetConnection::onErrorSlot( QAbstractSocket::SocketError error )
{
    qDebug() << "Error: " << error << ".  " << m_Socket.errorString();
}

void VNetConnection::onSendMessage(ProtocolMessage_t *message )
{
    qint64 bytesSent = 0;
    quint64 bytesSentTotal = 0;
    quint64 bytesLeftToSend = message->length;

    //endian conversion
    message->length = qToBigEndian<quint32>( message->length );
    message->token = qToBigEndian<quint32>( message->token );
    message->type = qToBigEndian<quint32>( message->type );

    //Send the all the bytes in this message
    while( bytesLeftToSend && m_Socket.state() == QTcpSocket::ConnectedState )
    {
        bytesSent = m_Socket.write( reinterpret_cast<char*>( message ) + bytesSentTotal, bytesLeftToSend );

        //Sanity check
        if( bytesSent < 0 )
        {
            QString errorMessage = m_Socket.errorString();
            qDebug() << "Failed to send message: " << errorMessage;
            return;
        }

        //Adjust counters
        bytesSentTotal += bytesSent;
        bytesLeftToSend -= bytesSent;

        //Update throughput statistics
        m_OutgoingByteCount += bytesSent;

        //Wait until the bytes are written
        m_Socket.waitForBytesWritten( 10000 );
        m_Socket.flush();
//        if( m_Socket.bytesToWrite() > 0 )
//        {
//            qDebug() << "We still have " << m_Socket.bytesToWrite() << " bytes left to write.";
//        }
//        while( m_Socket.bytesToWrite() > 0 )
//        {
//            QApplication::processEvents();
//            m_Socket.flush();
//            QThread::yieldCurrentThread();
//            QThread::msleep( 20 );
//        }
    }
}

void VNetConnection::onConnectedSlot()
{
    qDebug() << "Connected to server";

    //Reset the book keeping
    m_TotalBytesLeftToRead = 0;
    m_TotalBytesRead = 0;
    memset( m_IncomingMessageBuffer, 0, INPUT_BUFFER_SIZE );

    //Inform our owner of the new state
    emit connectedToHostSignal();
}

void VNetConnection::onDisconnectedSlot()
{
    qDebug() << "Disconnected from server";
    emit disconnectedFromHostSignal();
}

void VNetConnection::onReadReadySlot()
{
    qint64 bytesRead = 0;

    //Are we in raw socket mode?
    if( m_RawSocketModeActive )
    {
        QByteArray bytes = m_Socket.readAll();
        emit rawIncomingBytesSignal( bytes );
        return;
    }


    //Do we need to get the header?
    if( m_TotalBytesLeftToRead == 0 )
    {
        //qDebug() << "New message arrived.";

        //Clear out the memory
        memset( m_IncomingMessageBuffer, 0, INPUT_BUFFER_SIZE );
        m_TotalBytesRead = 0;

        //We really want to wait until we have the full header
        if( m_Socket.bytesAvailable() < 12 )
            return;

        //read single bytes until we find the start of the magic token
        qint32 badByteCount = 0;
        while( m_Socket.bytesAvailable() )
        {
            m_Socket.peek( reinterpret_cast<char*>( &m_IncomingMessageBuffer->token ), sizeof( m_IncomingMessageBuffer->token ) );
            m_IncomingMessageBuffer->token = qFromBigEndian<quint32>( m_IncomingMessageBuffer->token );
            //qDebug() << "Searching for token: " << QByteArray( reinterpret_cast<char*>( &m_IncomingMessageBuffer->token), 4).toHex();
            if( m_IncomingMessageBuffer->token == MAGIC_TOKEN )
            {

                char dummyBytes[ 4 ];
                bytesRead = m_Socket.read( dummyBytes, sizeof( m_IncomingMessageBuffer->token ) );
                m_TotalBytesRead += bytesRead;
                break;
            }

            //Jump to the next byte
            char dummyByte;     //Throw-away byte
            m_Socket.read( &dummyByte, 1 );
            badByteCount++;
        };
        if( badByteCount )
            qDebug() << "Took " << badByteCount << " bytes to find the token of the next message.";

        //Check if the token is valid
        if( m_IncomingMessageBuffer->token != MAGIC_TOKEN )
        {
            qDebug() << "Invalid token on new message.  Aborting.";
            m_Socket.disconnectFromHost();
            return;
        }

        //Now read and convert the type
        bytesRead = m_Socket.read( reinterpret_cast<char*>( &m_IncomingMessageBuffer->type ), sizeof( m_IncomingMessageBuffer->type ) );
        if( bytesRead != sizeof( m_IncomingMessageBuffer->type ) )
        {
            qDebug() << "Unable to read the message type.  Aborting.";
            m_Socket.disconnectFromHost();
            return;
        }
        m_TotalBytesRead += bytesRead;
        m_IncomingMessageBuffer->type = qFromBigEndian( m_IncomingMessageBuffer->type );

        //Now read the message length
        bytesRead = m_Socket.read( reinterpret_cast<char*>( &m_IncomingMessageBuffer->length ), sizeof( m_IncomingMessageBuffer->length ) );
        if( bytesRead != sizeof( m_IncomingMessageBuffer->length ) )
        {
            qDebug() << "Unable to read the message length.  Aborting.";
            m_Socket.disconnectFromHost();
            return;
        }
        m_TotalBytesRead += bytesRead;
        m_IncomingMessageBuffer->length = qFromBigEndian<quint32>( m_IncomingMessageBuffer->length );

        //Reset book keeping
        m_TotalBytesLeftToRead = m_IncomingMessageBuffer->length - m_TotalBytesRead;
    }

    //Read what bytes we have left
    while( m_Socket.bytesAvailable() && m_TotalBytesLeftToRead )
    {
        bytesRead = m_Socket.read( reinterpret_cast<char*>( m_IncomingMessageBuffer ) + m_TotalBytesRead, m_TotalBytesLeftToRead );
        if( bytesRead < 0 )
        {
            qDebug() << "Socket error on read.  Aborting connection.";
            m_Socket.disconnectFromHost();
            return;
        }
        m_TotalBytesLeftToRead -= bytesRead;
        m_TotalBytesRead += bytesRead;

        //Update the statistics
        m_IncomgingByteCount += bytesRead;
    }

    //have we read all bytes?
    if( m_TotalBytesLeftToRead )
        return;     //Nope, we need more.

    //Debug
#if 0
    qDebug() << "Message.header.token: " << QByteArray( reinterpret_cast<char*>( &m_IncomingMessageBuffer->token), 4).toHex();
    qDebug() << "Message.header.type: " << QByteArray( reinterpret_cast<char*>( &m_IncomingMessageBuffer->type), 4).toHex();
    qDebug() << "Message.header.length: " << m_IncomingMessageBuffer->length;
#endif

    //If we have a completed message now, we should send that on
    emit newMessageReceived( m_IncomingMessageBuffer );

    //Now reset the book keeping ready for the next message
    m_TotalBytesLeftToRead = 0;
    m_TotalBytesRead = 0;
    memset( m_IncomingMessageBuffer, 0, INPUT_BUFFER_SIZE );

    //If there are still bytes to read on the socket, call this function again
    if( m_Socket.bytesAvailable() > 0 )
        onReadReadySlot();
}

void VNetConnection::onThroughputTimerExpiredSlot()
{
    //Emit the current byte count
    emit outgoingByteCountSignal( m_OutgoingByteCount );
    emit incomingByteCountSignal( m_IncomgingByteCount );

    //reset the stats
    m_OutgoingByteCount = 0;
    m_IncomgingByteCount = 0;
}

void VNetConnection::onSetRawSocketMode()
{
    m_RawSocketModeActive = true;
}

void VNetConnection::onRawOutgoingBytesSlot(QByteArray bytes)
{
    m_Socket.write( bytes );
}
