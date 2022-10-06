#ifndef UPLOADTHREAD_H
#define UPLOADTHREAD_H

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

class UploadThread : public QThread
{
    Q_OBJECT

private:
    typedef enum
    {
        JT_NONE,
        JT_UPLOAD,
        JT_UPLOAD_INIT,
        JT_UPLOAD_WAIT_ACK,
        JT_MKDIR,
        JT_UNKNOWN
    } JobType;

public:
    typedef enum
    {
        UF_NONE,
        UF_SIZE_MISMATCH,
        UF_CHECKSUM_MISMATCH,
        UF_TIMEOUT,
        UF_UNKNOWN
    } UploadFailureType;


public:
    explicit UploadThread(QObject *parent = nullptr);

    void run() override final;

    void stopThread();

public slots:
    void onConnectToHostSlot( QHostAddress host, quint16 port );
    void onDisconnectFromHostRequestedSlot();
    void onStartFileSlot( QString localFilePath, QString remoteFilePath );
    void onCancelUploadSlot();
    void onFileChunkReceivedSlot( quint32 chunkNumber );
    void onFilePutConfirmedSlot( quint32 sizeWritten );
    void onCreateDirectorySlot( QString remotePath );

    //Internally used slots
    void onConnectedToHostSlot();
    void onDisconnectedFromHostSlot();
    void onIncomingBytesUpdateSlot( quint32 bytes );
    void onOutgoingBytesUpdateSlot( quint32 bytes );
    void onAcknowledgeSlot( quint8 responseCode );
    void onThroughputTimerExpiredSlot();
    void onUploadTimeoutTimerExpiredSlot();

private:
    void cleanup();

signals:
    void uploadCompletedSignal();
    void uploadFailedSignal( UploadFailureType failure );
    void uploadProgressSignal( quint8 percentPercent, quint64 progressBytes, quint64 thoughput );
    void outgoingBytesSignal( quint32 bytesSent );
    void disconnectedFromServerSignal();
    void connectedToServerSignal();
    void abortedSignal( QString message );
    void directoryCreationCompletedSignal();
    void directoryCreationFailedSignal();
    void operationTimedOutSignal();

    //Communication with the protocol handler
    void sendMessageSignal( ProtocolMessage_t *message );
    void sendAndReleaseMessageSignal( ProtocolMessage_t *message );
    void connectToHostSignal( QHostAddress host, quint16 port );
    void disconnectFromHostSignal();
    void createDirectorySignal( QString remotePath );

    //Timer communications
    void startUploadTimeoutTimerSignal();
    void stopUploadTimeoutTimerSignal();

private:
    QMutex m_Mutex;
    QTimer *m_ThroughPutTimer;
    QTimer *m_UploadTimeoutTimer;
    ProtocolHandler *m_ProtocolHandler;
    ProtocolHandler::AcknowledgeState m_AcknowledgeState;
    bool m_Connected;
    bool m_FileToUpload;
    JobType m_JobType;
    QString m_RemoteFilePath;
    QString m_LocalFilePath;
    QFile m_LocalFile;
    quint64 m_FileSize;
    quint64 m_ProgressBytes;
    quint8 m_ProgressProcent;
    quint64 m_FileChunks;
    quint64 m_CurrentChunk;
    QAtomicInteger<quint64> m_BytesSentThisSecond;
    QAtomicInteger<quint64> m_BytesReceivedThisSecond;
    QAtomicInteger<quint64> m_ThroughPut;
    QAtomicInteger<bool> m_Keeprunning;
    QList<quint32> m_InflightChunks;
    quint32 m_MaxInflightChunks;
};

#endif // UPLOADTHREAD_H
