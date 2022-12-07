#ifndef DELETIONTHREAD_H
#define DELETIONTHREAD_H

#include <QObject>
#include <QThread>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QFile>
#include <QTimer>
#include <QAtomicInteger>

#include "aeconnection.h"
#include "protocolhandler.h"
#include "messagepool.h"
class DeletionThread : public QThread
{
    Q_OBJECT
public:
    explicit DeletionThread(QObject *parent = nullptr);

    void run() override final;

    void stopThread();

public slots:
    void onConnectToHostSlot( QHostAddress host, quint16 port );
    void onDisconnectFromHostRequestedSlot();
    void onDeleteFileSlot( QString path );
    void onDeleteRecursiveSlot( QString path );

    //Internally used slots
    void onConnectedToHostSlot();
    void onDisconnectedFromHostSlot();
    void onTimeoutSlot();
    void onFileDeletedSlot( QString path );
    void onFileDeleteFailedSlot( QString path, DeleteFailureReason reason );
    void onRecursiveDeleteionCompletedSlot();

signals:
    void fileDeletedSignal( QString path );
    void fileDeleteFailedSignal( QString path, DeleteFailureReason reasonCode );
    void recursiveDeletionCompletedSignal();
    void deletionTimedoutSignal();
    void startTimeoutTimerSignal();
    void stopTimeoutTimerSignal();


private:
    QMutex m_Mutex;
    QTimer *m_TimeoutTimer;
    ProtocolHandler *m_ProtocolHandler;
   QAtomicInteger<bool> m_RecursiveDeleteActive;
   QAtomicInteger<bool> m_Keeprunning;
};

#endif // DELETIONTHREAD_H
