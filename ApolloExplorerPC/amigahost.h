#ifndef AMIGAHOST_H
#define AMIGAHOST_H

#include <QObject>
#include <QHostAddress>
#include <QTime>
#include <QSharedPointer>
#include <QSettings>

Q_DECLARE_METATYPE(QHostAddress)

class AmigaHost : public QObject
{
    Q_OBJECT

public:
    typedef enum HardwareType {
        HARDWARE_TYPE_UNKNOWN = 0x00,
        HARDWARE_TYPE_V600 = 0x01,
        HARDWARE_TYPE_V500 = 0x02,
        HARDWARE_TYPE_FIREBIRD = 0x03,
        HARDWARE_TYPE_ICEDRAKE = 0x04,
        HARDWARE_TYPE_V4 = 0x05,
        HARDWARE_TYPE_V1200 = 0x06,
        HARDWARE_TYPE_MANTICORE= 0x07,
        HARDWARE_TYPE_UNICORN = 0x08
    } HardwareType;

public:
    AmigaHost( quint32 timeoutInSeconds, QString name, QString osName, QString osVersion, HardwareType hardware, QHostAddress address, bool isStaticallyConfigured = false, QObject *parent = nullptr );
    bool operator == ( const AmigaHost &rhs );

    bool hasTimedOut();
    void setHostRespondedNow();

    const QString &Name() const;

    const QString &OsName() const;

    const QString &OsVersion() const;

    const QString HardwareName();

    const HardwareType &Hardware() const;

    const QHostAddress &Address() const;

    static QPixmap getPixmap( HardwareType &type );

    static QString hardwareTypeAsString( HardwareType &type );

    static HardwareType hardwareTypeFromString( QString &name );

    bool StaticlyConfiguredHost() const;

signals:

private:
    QString m_Name;
    QString m_OsName;
    QString m_OsVersion;
    HardwareType m_Hardware;
    QHostAddress m_Address;
    QTime m_TimeoutTime;
    quint32 m_TimeoutInSeconds;
    bool m_StaticlyConfiguredHost;      //If this host is from the static configuration and not one that is discovered

};

#endif // AMIGAHOST_H
