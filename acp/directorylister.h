#ifndef DIRECTORYLISTER_H
#define DIRECTORYLISTER_H

#include <QObject>
#include <QSharedPointer>

#include "amigahost.h"
#include "protocolhandler.h"

class DirectoryLister : public QObject
{
    Q_OBJECT
public:
    explicit DirectoryLister( QString ipAddressString, QString path, QObject *parent = nullptr);
    QStringList getDirectoryList();

public slots:
    void onVolumeListSlot( QList<QSharedPointer<DiskVolume>> volumes );

private:
    QString m_Host;
    QString m_Path;
    ProtocolHandler m_ProtocolHander;
    QList<QSharedPointer<DiskVolume>> m_Volumes;
};

#endif // DIRECTORYLISTER_H
