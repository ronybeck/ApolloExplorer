#ifndef FILEDOWNLOADER_H
#define FILEDOWNLOADER_H

#include "downloadthread.h"

#include <QObject>
#include <memory>

class FileDownloader : public QObject
{
    Q_OBJECT
public:
    explicit FileDownloader( QString remoteSources, QString localDestination, QString remoteHost, QObject *parent = nullptr );
    ~FileDownloader();

    bool connectToHost();
    void disconnectFromHost();
    bool startDownload();
    void setRecursive( bool enabled );


public slots:
    void onDownloadCompletedSlot();
    void ondownloadProgressSlot( quint8 percent, quint64 progressBytes, quint64 throughput );
    void onDisconnectedFromServerSlot();
    void onConnectedToServerSlot();
    void onDirectoryListingSlot( QSharedPointer<DirectoryListing> listing );
    void onAbortSlot( QString reason );
    void onOperationTimedoutSlot();

signals:
    void downloadCompletedSignal();
    void startDownloadSignal( QString localPath, QString remotePath );
    void downloadAbortedSignal( QString reason );

private:
    bool getNextDirectory();

private:
    std::unique_ptr<DownloadThread> m_DownloadThread;
    QString m_RemoteSource;
    QString m_RemoteSourceParent;
    QString m_LocalDestination;
    QHostAddress m_RemoteHost;
    QStringList m_RemoteDirectoriesToProcess;
    QMap<QString,QSharedPointer<DirectoryListing>> m_DownloadList;
    bool m_Recusive;
    QString m_CurrentRemotePath;
    QString m_CurrentLocalPath;
    QString m_FileMatchPattern;
};

#endif // FILEDOWNLOADER_H
