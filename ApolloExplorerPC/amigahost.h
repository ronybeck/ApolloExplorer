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
    explicit AmigaHost( QSharedPointer<QSettings> settings, QString name, QString osName, QString osVersion, QString hardware, QHostAddress address, QObject *parent = nullptr );
    bool operator == ( const AmigaHost &rhs );

    bool hasTimedOut();
    void setHostRespondedNow();

    const QString &Name() const;

    const QString &OsName() const;

    const QString &OsVersion() const;

    const QString &Hardware() const;

    const QHostAddress &Address() const;

signals:

private:
    QString m_Name;
    QString m_OsName;
    QString m_OsVersion;
    QString m_Hardware;
    QHostAddress m_Address;
    QTime m_TimeoutTime;
    QTime m_LastRepsonseTime;
    QSharedPointer<QSettings> m_Settings;
};

#endif // AMIGAHOST_H
