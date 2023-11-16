#ifndef FILEUPLOADER_H
#define FILEUPLOADER_H

#include "uploadthread.h"

#include <memory>
#include <QObject>
#include <QHostAddress>

class FileUploader : public QObject
{
    Q_OBJECT
public:
    explicit FileUploader( QStringList localSources, QString remoteDestination, QString remoteHost, QObject *parent = nullptr);
    ~FileUploader();

    bool connectToHost();
    void disconnectFromHost();
    bool startUpload();
    void setRecursive( bool enabled );

signals:
    //Signals back to the parent
    void uploadCompletedSignal();
    void uploadAbortedSignal( QString reason );

    //Signals to the uploader thread
    void startFileSignal( QString localPath, QString remotePath );
    void createDirectorySignal( QString remotePath );

public slots:
    void fileUploadCompletedSlot();
    void fileUploadFailedSlot( UploadThread::UploadFailureType failure );
    void uploadProgressSlot( quint8 percentPercent, quint64 progressBytes, quint64 thoughput );
    void connectedToServerSlot();
    void disconnectedFromServerSlot();
    void directoryCreationCompletesSlot();
    void directoryCreationFailedSlot();


private:
    void startNextFile();


private:
    QStringList m_SourcePaths;
    QString m_DestinationPath;
    QHostAddress m_DestinationHost;
    std::unique_ptr<UploadThread> m_UploadThread;
    QMap<QString,QString> m_FileList;
    QString m_CurrentLocalPath;
    QString m_CurrentRemotePath;
    bool m_Recursive;
};

#endif // FILEUPLOADER_H
