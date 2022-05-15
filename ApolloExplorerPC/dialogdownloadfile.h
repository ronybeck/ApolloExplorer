#ifndef DIALOGDOWNLOADFILE_H
#define DIALOGDOWNLOADFILE_H

#include <QDialog>
#include <QTimer>
#include <QDebug>
#include <QFile>
#include <QMutexLocker>
#include <QThread>
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
    explicit DialogDownloadFile(QWidget *parent = nullptr);
    ~DialogDownloadFile();

    //Caller Commands
    void connectToHost( QHostAddress, quint16 port );
    void disconnectFromhost();
    void startDownload( QList<QPair<QString,QString>> files );
    void startDownload( QList<QSharedPointer<DirectoryListing>> remotePaths, QString localPath );
    bool isCurrentlyDownloading();

public slots:
    //GUI Slots
    void onCancelButtonReleasedSlot();

    //Internally used slots
    void onConnectedToHostSlot();
    void onDisconnectedFromHostSlot();
    void onAbortedSlot( QString reason );
    void onDownloadCompletedSlot();
    void onProgressUpdate( quint8 procent, quint64 bytes, quint64 throughput );

signals:
    void downloadCompletedSignal();
    void startDownloadSignal( QString localFilePath, QString remoteFilePath );

private:
    void resetDownloadDialog();
    QList<QPair<QString,QString>> prepareDownload( QString localPath, QString remotePath, bool &error );

private:
    Ui::DialogDownloadFile *ui;
    DownloadThread m_DownloadThread;
    QList<QPair<QString, QString>> m_DownloadList;
    QMutex m_Mutex;
    QAtomicInteger<bool> m_DownloadActive;
};

#endif // DIALOGDOWNLOADFILE_H
