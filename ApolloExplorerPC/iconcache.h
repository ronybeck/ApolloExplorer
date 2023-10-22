#ifndef ICONCACHE_H
#define ICONCACHE_H

#include <QObject>
#include <QMap>
#include <QSharedPointer>
#include <QMutex>

#include "../AmigaIconReader/AmigaInfoFile.h"
#include "iconthread.h"
class IconCache : public QObject
{
    Q_OBJECT

public:
    explicit IconCache(QObject *parent = nullptr);

    void clearCache();

    ~IconCache();

    //Get icons if they exist already
    QSharedPointer<AmigaInfoFile> getIcon( QString filepath );

public slots:
    //External Slots
    void onConnectToHostSlot( QHostAddress host, quint16 port );
    void onDisconnectFromHostSlot( );
    void retrieveIconSlot( QString filepath );

    //Slots with the icon thread
    void onConnectedToHostSlot();
    void onDisconnectedFromHostSlot();
    void onIconReceivedSlot( QString filepath, QSharedPointer<AmigaInfoFile> icon );
    void onOperationTimedOut();
    void onAbortSlot( QString reason );

signals:
    void iconSignal( QString filepath, QSharedPointer<AmigaInfoFile> icon );
    void disconnectedFromHostSignal();
    void connectedToHostSignal();

    //Communication with the thread
    void connectToHostSignal( QHostAddress host, quint16 port );
    void disconnectFromHostSignal();
    void getIconSignal( QString filePath );

private:
    QMutex m_Mutex;
    QMap<QString,QSharedPointer<AmigaInfoFile>> m_Cache;
    QStringList m_DownloadList;
    bool m_Connected;
    bool m_DownloadInProgress;
    IconThread *m_IconThread;
    QString m_RamDiskIconPath;      //Annoyingly, the ram disk has slight differences in the case between Amiga OS versions
};

#endif // ICONCACHE_H
