#ifndef REMOTEFILEMIMEDATA_H
#define REMOTEFILEMIMEDATA_H

#include "dialogdownloadfile.h"

#include <QMimeData>
#include <QObject>
#include <QDrag>
#include <QFile>
#include <QByteArray>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QEventLoop>
#include <QAbstractNativeEventFilter>
#include <QMutexLocker>
#include <QMetaType>
#include <QRecursiveMutex>


class RemoteFileMimeData : public QMimeData
{
    Q_OBJECT
public:
    RemoteFileMimeData();
    ~RemoteFileMimeData();

    QVariant retrieveData( const QString &mimeType, QMetaType type ) const override;
    virtual bool hasFormat(const QString &mimeType) const override;

    void setAction( Qt::DropAction action );
    void setTempFilePath( const QString& path );
    void addRemotePath( const QSharedPointer<DirectoryListing> remotePath );
    void setDownloadDialog( QSharedPointer<DialogDownloadFile> dialog );
    bool isLeftMouseButtonDown() const;
    void setSendURLS( bool enabled );

public slots:
    void onLeftMouseButtonPressedSlot();
    void onLeftMouseButtonReleasedSlot();

signals:
    void dropHappenedSignal() const;

private:
    mutable QRecursiveMutex m_Mutex;
    Qt::DropActions m_Action;
    QString m_TempFilePath;
    mutable QList<QUrl> m_LocalUrls;
    QList<QPair<QString,QString>> m_DownloadList;
    QSharedPointer<DialogDownloadFile> m_DownloadDialog;
    QList<QSharedPointer<DirectoryListing>> m_RemotePaths;
    bool m_SendURLS;

protected:
    bool m_LeftMouseButtonDown;
    mutable bool m_DataRetrieved;
};

#endif // REMOTEFILEMIMEDATA_H
