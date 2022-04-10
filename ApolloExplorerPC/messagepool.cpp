#include "messagepool.h"

#include <QDebug>

#define MESSAGE_POOL_SIZE 4

#define LOCK QMutexLocker locker( &m_Mutex )
#define UNLOCK locker.unlock()
#define RELOCK locker.lock()

MessagePool *MessagePool::getInstance()
{
    static MessagePool *instance = new MessagePool();
    return instance;
}

ProtocolMessage_t *MessagePool::newMessage()
{
    LOCK;

    //qDebug() << "MessagePool: " << m_UsedList.size() << " used.  " << m_FreeList.size() << " free.";

    while( m_FreeList.isEmpty() )
    {
        //Set up a wait for the next free message
        UNLOCK;
        qDebug() << "MessagePool empty.  Consider increasing it.";
        m_WaitMutex.lock();
        m_WaitCondition.wait( &m_WaitMutex );

        //Hopefully we can get some memory now
        LOCK;
        if( !m_FreeList.empty() )
            break;
        //If not, loop again
    }

    ProtocolMessage_t *msg = m_FreeList.front();
    m_FreeList.pop_front();
    m_UsedList.push_back( msg );
    return msg;
}

void MessagePool::freeMessage(ProtocolMessage_t *message)
{
    LOCK;
    Q_ASSERT(m_UsedList.indexOf( message ) > -1 );
    int indexOfUsedMessage = m_UsedList.indexOf( message );
    if( indexOfUsedMessage >= m_UsedList.count() )
    {
        qDebug() << "Somehow the index of the used message (" << indexOfUsedMessage << ") is outside of the used message list of " << m_UsedList.count();
    }else
    {
        m_UsedList.removeAt( indexOfUsedMessage );
    }
    m_FreeList.push_back( message );

    //Notify any waiting thread that memory is available
    m_WaitCondition.notify_all();
}

MessagePool::MessagePool() :
    m_Mutex( QMutex::Recursive ),
    m_WaitMutex( ),
    m_WaitCondition( ),
    m_FreeList( ),
    m_UsedList( )
{
    //Initialise the pool of messages
    for( int i = 0; i < MESSAGE_POOL_SIZE; i++ )
    {
        ProtocolMessage_t *msg = static_cast<ProtocolMessage_t*>( calloc( 1, MAX_MESSAGE_LENGTH ) );
        m_FreeList.push_back( msg );
    }
}

MessagePool::~MessagePool()
{
    while( !m_FreeList.isEmpty() )
    {
        free( m_FreeList.front() );
        m_FreeList.pop_front();
    }

    while( !m_UsedList.isEmpty() )
    {
        free( m_UsedList.front() );
        m_UsedList.pop_front();
    }
}
