#ifndef DIALOGUPLOADFILE_H
#define DIALOGUPLOADFILE_H

#include <QDialog>
#include <QTimer>
#include <QDebug>
#include <QFile>
#include <QWaitCondition>
#include <QThread>

#include <uploadthread.h>

namespace Ui {
class DialogUploadFile;
}

class DialogUploadFile : public QDialog
{
    Q_OBJECT

public:
    explicit DialogUploadFile(QWidget *parent = nullptr);
    ~DialogUploadFile();

    //Caller Commands
    void connectToHost( QHostAddress host, quint16 port );
    void disconnectFromhost();
    void startUpload( QList<QPair<QString,QString>> files );

public slots:
    void onCancelButtonReleasedSlot();

    //Internally used slots
    void onConnectedToHostSlot();
    void onDisconnectedFromHostSlot();
    void onUploadCompletedSlot();
    void onAbortedSlot( QString reason );
    void onProgressUpdate( quint8 procent, quint64 bytes, quint64 throughput );

signals:
    void uploadCompletedSignal();
    void startupUploadSignal( QString localFilePath, QString remoteFilePath );

private:
    Ui::DialogUploadFile *ui;
    UploadThread m_UploadThread;
    QList<QPair<QString, QString>> m_UploadList;
    QString m_CurrentLocalFilePath;
    QString m_CurrentRemoteFilePath;
};

#endif // DIALOGUPLOADFILE_H
