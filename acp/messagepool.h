#ifndef MESSAGEPOOL_H
#define MESSAGEPOOL_H

#include <QObject>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QVector>

#include "protocolTypes.h"

#define NewMessage() MessagePool::getInstance()->newMessage()
#define FreeMessage( x ) MessagePool::getInstance()->freeMessage( x )

class MessagePool
{
public:
    static MessagePool *getInstance();
    ProtocolMessage_t *newMessage();
    void freeMessage( ProtocolMessage_t *message );

private:
    MessagePool();
    ~MessagePool();

private:
    QMutex m_Mutex;
    QMutex m_WaitMutex;
    QWaitCondition m_WaitCondition;
    QVector<ProtocolMessage_t*> m_FreeList;
    QVector<ProtocolMessage_t*> m_UsedList;
};

template<typename T> T *AllocMessage()
{
    return reinterpret_cast<T*>( MessagePool::getInstance()->newMessage() );
}

template<typename T>
void ReleaseMessage( T *msg )
{
    MessagePool::getInstance()->freeMessage( reinterpret_cast<ProtocolMessage_t*>( msg ) );
}

#endif // MESSAGEPOOL_H
