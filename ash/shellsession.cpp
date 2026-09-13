#include "shellsession.h"

#include <iostream>
#include <QThread>
#include <QCoreApplication>
#include <QtEndian>

#define DEBUG 0
#include "AEUtils.h"
#include "messagepool.h"

ShellSession::ShellSession( QString ipAddressString, QObject *parent )
    : QObject{ parent },
      m_Host( ipAddressString ),
      m_ProtocolHandler( this ),
      m_CommandDone( false ),
      m_ReturnCode( 0 ),
      m_CompletionDone( false )
{
    connect( &m_ProtocolHandler, &ProtocolHandler::shellOutputSignal,
             this, &ShellSession::onShellOutputSlot );
    connect( &m_ProtocolHandler, &ProtocolHandler::shellDoneSignal,
             this, &ShellSession::onShellDoneSlot, Qt::DirectConnection );
    connect( &m_ProtocolHandler, &ProtocolHandler::shellCompleteRspSignal,
             this, &ShellSession::onShellCompleteRspSlot, Qt::DirectConnection );
}

bool ShellSession::connectToHost()
{
    m_ProtocolHandler.onConnectToHostRequestedSlot( QHostAddress( m_Host ), MAIN_LISTEN_PORTNUMBER );

    bool connected = false;
    QObject::connect( &m_ProtocolHandler, &ProtocolHandler::connectedToHostSignal, [&]() {
        connected = true;
    });

    for( int i = 0; i < 1000; i++ )
    {
        if( connected ) break;
        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    return connected;
}

void ShellSession::disconnectFromHost()
{
    m_ProtocolHandler.onDisconnectFromHostRequestedSlot();
    QThread::msleep( 200 );
    QCoreApplication::processEvents();
}

int ShellSession::execute( const QString &command )
{
    m_CommandDone = false;
    m_ReturnCode = 0;

    QByteArray encodedCommand = command.toLatin1();
    int cmdLen = encodedCommand.size() + 1;
    int msgLen = sizeof( ProtocolMessage_t ) + cmdLen;

    QByteArray msgBuf( msgLen, 0 );
    ProtocolMessage_ShellExec_t *msg =
        reinterpret_cast<ProtocolMessage_ShellExec_t*>( msgBuf.data() );
    msg->header.token  = MAGIC_TOKEN;
    msg->header.type   = PMT_SHELL_EXEC;
    msg->header.length = msgLen;
    memcpy( msg->command, encodedCommand.constData(), cmdLen );

    m_ProtocolHandler.sendMessage( msg );

    while( !m_CommandDone )
    {
        for( const QByteArray &chunk : m_PendingOutput )
            std::cout.write( chunk.constData(), chunk.size() );
        m_PendingOutput.clear();

        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    for( const QByteArray &chunk : m_PendingOutput )
        std::cout.write( chunk.constData(), chunk.size() );
    m_PendingOutput.clear();
    std::cout.flush();

    return m_ReturnCode;
}

QStringList ShellSession::requestCompletion( const QString &partialToken )
{
    m_CompletionDone = false;
    m_Completions.clear();

    QByteArray encoded = partialToken.toLatin1();
    QByteArray msgBuf( sizeof( ProtocolMessage_ShellCompleteReq_t ), 0 );
    ProtocolMessage_ShellCompleteReq_t *msg =
        reinterpret_cast<ProtocolMessage_ShellCompleteReq_t*>( msgBuf.data() );
    msg->header.token  = MAGIC_TOKEN;
    msg->header.type   = PMT_SHELL_COMPLETE_REQ;
    msg->header.length = sizeof( ProtocolMessage_ShellCompleteReq_t );
    strncpy( msg->partial, encoded.constData(), sizeof( msg->partial ) - 1 );

    m_ProtocolHandler.sendMessage( msg );

    for( int i = 0; i < 500 && !m_CompletionDone; i++ )
    {
        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    return m_Completions;
}

void ShellSession::onShellOutputSlot( quint32 bytesContained, QByteArray data )
{
    Q_UNUSED( bytesContained )
    m_PendingOutput.append( data );
}

void ShellSession::onShellDoneSlot( int returnCode, QString currentDir )
{
    m_ReturnCode = returnCode;
    m_CurrentDir = currentDir;
    m_CommandDone = true;
}

void ShellSession::onShellCompleteRspSlot( QStringList completions )
{
    m_Completions = completions;
    m_CompletionDone = true;
}
