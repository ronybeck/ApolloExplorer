#ifndef ICONTHREAD_H
#define ICONTHREAD_H

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
#include "../AmigaIconReader/AmigaInfoFile.h"

class IconThread : public QThread
{
    Q_OBJECT
public:
    enum ConnectionState
    {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        DISCONNECTING,
        UNKNOWN_CONNECTION_STATE
    };

    enum OperationState
    {
        IDLE,
        DOWNLOADING,
        GETTING_DIRECTORY,
        UNKNOWN_OPERATION_STATE
    };


    explicit IconThread(QObject *parent = nullptr);

    void run() override final;

    void stopThread();

public slots:
    void onConnectToHostSlot( QHostAddress host, quint16 port );
    void onDisconnectFromHostRequestedSlot();
    void onGetIconSlot( QString remoteFilePath );
    void onCancelDownloadSlot();
    void onOperationTimerTimeoutSlot();


    //Internally used slots
    void onConnectedToHostSlot();
    void onDisconnectedFromHostSlot();
    void onIncomingBytesUpdateSlot( quint32 bytes );
    void onOutgoingBytesUpdateSlot( quint32 bytes );
    void onAcknowledgeSlot( quint8 responseCode );
    void onStartOfFileSendSlot( quint64 fileSize, quint32 numberOfChunks, QString filename );
    void onFileChunkSlot( quint32 chunkNumber, quint32 bytes, QByteArray chunk );

private:
    void cleanup();

signals:
    void downloadCompletedSignal();
    void disconnectedFromServerSignal();
    void connectedToServerSignal();
    void abortedSignal( QString message );

    //Communication with the protocol handler
    void sendMessageSignal( ProtocolMessage_t *message );
    void sendAndReleaseMessageSignal( ProtocolMessage_t *message );
    void connectToHostSignal( QHostAddress host, quint16 port );
    void disconnectFromHostSignal();

    //Operation timer signals
    void startOperationTimerSignal();
    void stopOperationTimerSignal();
    void operationTimedOutSignal();
    void iconFileSignal( QString filePath, QSharedPointer<AmigaInfoFile> icon );


private:
    QMutex m_Mutex;
    QTimer *m_OperationTimer;
    ProtocolHandler *m_ProtocolHandler;
    QSharedPointer<DirectoryListing> m_DirectoryListing;
    OperationState m_OperationState;
    ConnectionState m_ConnectionState;
    QString m_RemoteFilePath;
    quint64 m_FileSize;
    quint64 m_ProgressBytes;
    quint8 m_ProgressProcent;
    quint64 m_FileChunks;
    quint64 m_CurrentChunk;
    QAtomicInteger<quint64> m_BytesSentThisSecond;
    QAtomicInteger<quint64> m_BytesReceivedThisSecond;
    QAtomicInteger<quint64> m_ThroughPut;
    QAtomicInteger<quint64> m_Keeprunning;
    QByteArray m_FileStorage;
};

#endif // ICONTHREAD_H
