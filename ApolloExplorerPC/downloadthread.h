#ifndef DOWNLOADTHREAD_H
#define DOWNLOADTHREAD_H

#include <QObject>
#include <QThread>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QFile>
#include <QTimer>
#include <QAtomicInteger>

#include "vnetconnection.h"
#include "protocolhandler.h"
#include "messagepool.h"

class DownloadThread : public QThread
{
    Q_OBJECT
public:
    explicit DownloadThread(QObject *parent = nullptr);

    void run() override final;

public slots:
    void onConnectToHostSlot( QHostAddress host, quint16 port );
    void onDisconnectFromHostRequestedSlot();
    void onStartFileSlot( QString localFilePath, QString remoteFilePath );
    void onCancelDownloadSlot();

    //Internally used slots
    void onConnectedToHostSlot();
    void onDisconnectedFromHostSlot();
    void onIncomingBytesUpdateSlot( quint32 bytes );
    void onOutgoingBytesUpdateSlot( quint32 bytes );
    void onAcknowledgeSlot( quint8 responseCode );
    void onStartOfFileSendSlot( quint64 fileSize, quint32 numberOfChunks, QString filename );
    void onFileChunkSlot( quint32 chunkNumber, quint32 bytes, QByteArray chunk );
    void onThroughputTimerExpiredSlot();

private:
    void cleanup();

signals:
    void downloadCompletedSignal();
    void downloadProgressSignal( quint8 percentPercent, quint64 progressBytes, quint64 thoughput );
    void disconnectedFromServerSignal();
    void connectedToServerSignal();
    void abortedSignal( QString message );

    //Communication with the protocol handler
    void sendMessageSignal( ProtocolMessage_t *message );
    void sendAndReleaseMessageSignal( ProtocolMessage_t *message );
    void connectToHostSignal( QHostAddress host, quint16 port );
    void disconnectFromHostSignal();


private:
    QMutex m_Mutex;
    QTimer m_ThroughPutTimer;
    ProtocolHandler *m_ProtocolHandler;
    bool m_Connected;
    bool m_FileToDownload;
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
};

#endif // DOWNLOADTHREAD_H
