#ifndef DEVICEDISCOVERY_H
#define DEVICEDISCOVERY_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QMutexLocker>
#include <QSharedPointer>
#include <QHostAddress>
#include <QSettings>

#include "protocolTypes.h"
#include "amigahost.h"

class DeviceDiscovery : public QObject
{
    Q_OBJECT
public:
    explicit DeviceDiscovery(QSharedPointer<QSettings> settings, QObject *parent = nullptr);

public slots:
    void onScanTimerTimeoutSlot();
    void onSocketReadReadySlot();

signals:
    void hostAliveSignal( QSharedPointer<AmigaHost> );
    void hostDiedSignal( QSharedPointer<AmigaHost> );


private:
    QTimer m_ScanTimer;
    QUdpSocket m_Socket;
    QMap<QString, QSharedPointer<AmigaHost>> m_HostList;
    QMutex m_Mutex;
    QSharedPointer<QSettings> m_Settings;
};

#endif // DEVICEDISCOVERY_H
