#ifndef DIALOGDOWNLOADFILE_H
#define DIALOGDOWNLOADFILE_H

#include <QDialog>
#include <QTimer>
#include <QDebug>
#include <QFile>
#include <QMutexLocker>
#include <QThread>
#include <QWaitCondition>
#include <QAtomicInteger>

#include "downloadthread.h"
#include "aeconnection.h"
#include "protocolhandler.h"
#include "messagepool.h"

namespace Ui {
class DialogDownloadFile;
}

class DialogDownloadFile : public QDialog
{
    Q_OBJECT

public:
    enum OperationState
    {
        IDLE,
        GETTING_DIRECTORY,
        DOWNLOADING_FILES,
        UNKNOWN_OPERATION_STATE
    };

    enum ConnectionState
    {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        DISCONNECTING,
        UNKNOWN_CONNECTION_STATE
    };


    explicit DialogDownloadFile(QWidget *parent = nullptr);
    ~DialogDownloadFile();

    //Caller Commands
    void connectToHost( QHostAddress, quint16 port );
    void disconnectFromhost();
    void startDownload( QList<QPair<QString,QString>> files );
    void startDownload( QList<QSharedPointer<DirectoryListing>> remotePaths, QString localPath );
    void startDownloadAndOpen( QList<QSharedPointer<DirectoryListing>> remotePaths );
    bool isCurrentlyDownloading();
    void waitForDownloadToComplete();

public slots:
    //GUI Slots
    void onCancelButtonReleasedSlot();

    //Internally used slots
    void onConnectedToHostSlot();
    void onDisconnectedFromHostSlot();
    void onAbortedSlot( QString reason );
    void onSingleFileDownloadCompletedSlot();
    void onAllFileDownloadsCompletedSlot();
    void onProgressUpdate( quint8 procent, quint64 bytes, quint64 throughput );
    void onOperationTimedoutSlot();

signals:
    void singleFileDownloadCompletedSignal();
    void allFilesDownloaded();
    void startDownloadSignal( QString localFilePath, QString remoteFilePath );
    void incomingBytesSignal( quint32 bytesReceived );

private:
    void resetDownloadDialog();
    QList<QPair<QString,QString>> prepareDownload( QString localPath, QString remotePath, bool &error );

private:
    Ui::DialogDownloadFile *ui;
    DownloadThread m_DownloadThread;
    QList<QPair<QString, QString>> m_DownloadList;
    QMutex m_Mutex;
    QMutex m_DownloadCompletionMutex;
    QWaitCondition m_DownloadCompletionWaitCondition;
    QStringList m_FilesToOpen;      //Which files are to be opened when the download is completed
    OperationState m_OperationState;
    ConnectionState m_ConnectionState;
    QHostAddress m_Host;
    quint16 m_Port;
    QString m_CurrentLocalFile;
    QString m_CurrentRemoteFile;
};

#endif // DIALOGDOWNLOADFILE_H
