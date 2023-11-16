#include "deletionthread.h"

#include <QApplication>

#define LOCK QMutexLocker locker( &m_Mutex )
#define UNLOCK locker.unlock()
#define RELOCK locker.relock();

#define TIMEOUT_TIME 10000


DeletionThread::DeletionThread(QObject *parent)
    : QThread{parent},
      m_Mutex( QMutex::Recursive ),
      m_TimeoutTimer( nullptr ),
      m_RecursiveDeleteActive( false )
{

}

void DeletionThread::run()
{
    //Initialise the timeout timer
    m_TimeoutTimer = new QTimer();
    m_TimeoutTimer->setSingleShot( true );
    m_TimeoutTimer->setInterval( TIMEOUT_TIME );
    connect( m_TimeoutTimer, &QTimer::timeout, this, &DeletionThread::onTimeoutSlot );

    //Setup the protocol handler
    m_ProtocolHandler = new ProtocolHandler();
    connect( m_ProtocolHandler, &ProtocolHandler::connectedToHostSignal, this, &DeletionThread::onConnectedToHostSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::disconnectedFromHostSignal, this, &DeletionThread::onDisconnectedFromHostSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::fileDeletedSignal, this, &DeletionThread::fileDeletedSignal );
    connect( m_ProtocolHandler, &ProtocolHandler::fileDeletedSignal, this, &DeletionThread::onFileDeletedSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::fileDeleteFailedSignal, this, &DeletionThread::fileDeleteFailedSignal );
    connect( m_ProtocolHandler, &ProtocolHandler::fileDeleteFailedSignal, this, &DeletionThread::onFileDeleteFailedSlot );
    connect( m_ProtocolHandler, &ProtocolHandler::recursiveDeletionCompletedSignal, this, &DeletionThread::recursiveDeletionCompletedSignal );
    connect( m_ProtocolHandler, &ProtocolHandler::recursiveDeletionCompletedSignal, this, &DeletionThread::onRecursiveDeleteionCompletedSlot );
    connect( this, &DeletionThread::startTimeoutTimerSignal, m_TimeoutTimer, QOverload<>::of(&QTimer::start) );
    connect( this, &DeletionThread::stopTimeoutTimerSignal, m_TimeoutTimer, QOverload<>::of(&QTimer::stop) );

    //Start the thread loop
    m_Keeprunning = true;
    while( m_Keeprunning )
    {
        //Is this thread to be terminated?
        if( this->isInterruptionRequested() )
        {
            m_Keeprunning = false;
        }

        //Event pump
        QApplication::processEvents();

        //Sleep
        QThread::yieldCurrentThread();
        QThread::msleep( 10 );
    }

    //Shutdown the connection
    m_TimeoutTimer->stop();
    delete m_TimeoutTimer;
    m_TimeoutTimer = nullptr;
    delete m_ProtocolHandler;
    m_ProtocolHandler = nullptr;
}

void DeletionThread::stopThread()
{
    m_Keeprunning = false;
}

void DeletionThread::onConnectToHostSlot(QHostAddress host, quint16 port)
{
    m_ProtocolHandler->onConnectToHostRequestedSlot( host, port );
}

void DeletionThread::onDisconnectFromHostRequestedSlot()
{
    m_ProtocolHandler->onDisconnectFromHostRequestedSlot();
}

void DeletionThread::onDeleteFileSlot(QString path)
{
    m_ProtocolHandler->onDeleteFileSlot( path );
    emit startTimeoutTimerSignal();
}

void DeletionThread::onDeleteRecursiveSlot(QString path)
{
    m_ProtocolHandler->onDeleteRecursiveSlot( path );
    m_RecursiveDeleteActive = true;
    emit startTimeoutTimerSignal();
}

void DeletionThread::onConnectedToHostSlot()
{
    //Nothing to do yet
}

void DeletionThread::onDisconnectedFromHostSlot()
{
    //Nothing to do yet
}

void DeletionThread::onTimeoutSlot()
{
    emit deletionTimedoutSignal();
}

void DeletionThread::onFileDeletedSlot(QString path)
{
    Q_UNUSED( path )
    emit startTimeoutTimerSignal();
}

void DeletionThread::onFileDeleteFailedSlot(QString path, DeleteFailureReason reason)
{
    Q_UNUSED( path )
    Q_UNUSED( reason )
    emit stopTimeoutTimerSignal();
}

void DeletionThread::onRecursiveDeleteionCompletedSlot()
{
    emit stopTimeoutTimerSignal();
}
