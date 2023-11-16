#ifndef HOSTLISTER_H
#define HOSTLISTER_H

#include <amigahost.h>
#include <devicediscovery.h>

#include <QObject>
#include <QSettings>
#include <QSharedPointer>
#include <QTimer>

class HostLister : public QObject
{
    Q_OBJECT

public:
    explicit HostLister( QSharedPointer<QSettings> settings, QObject *parent = nullptr );
    ~HostLister();

    QStringList getHostLike( QString pattern );

signals:
    void hostsDiscoveredSignal( QVector<QSharedPointer<AmigaHost>> );

public slots:
    void onNewDeviceDiscoveredSlot( QSharedPointer<AmigaHost> host );
    void onDeviceLeftSlot(  QSharedPointer<AmigaHost> host  );
    void onScanningTimeoutSlot();

private:
    QSharedPointer<QSettings> m_Settings;
    DeviceDiscovery *m_DeviceDiscovery;
    QVector<QSharedPointer<AmigaHost>> m_HostList;
    quint32 m_HelloTimeout;
    QTimer *m_TimeoutTimer;
};

#endif // HOSTLISTER_H
