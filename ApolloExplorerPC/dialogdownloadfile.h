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
    void startDownloadAndOpen( QList<QSharedPointer<DirectoryListing>> remotePaths );
    bool isCurrentlyDownloading();

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

signals:
    void singleFileDownloadCompletedSignal();
    void allFilesDownloaded();
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
    QStringList m_FilesToOpen;      //Which files are to be opened when the download is completed
};

#endif // DIALOGDOWNLOADFILE_H
