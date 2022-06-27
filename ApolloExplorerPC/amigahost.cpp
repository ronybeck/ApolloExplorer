#include "amigahost.h"
#include "AEUtils.h"

AmigaHost::AmigaHost( QSharedPointer<QSettings> settings, QString name, QString osName, QString osVersion, QString hardware, QHostAddress address, QObject *parent ) :
    QObject(parent),
    m_Name( name ),
    m_OsName( osName ),
    m_OsVersion( osVersion ),
    m_Hardware( hardware ),
    m_Address( address ),
    m_Settings( settings )
{
    m_TimeoutTime = QTime::currentTime().addSecs( 20 );
}

bool AmigaHost::operator ==( const AmigaHost &rhs )
{
    if( rhs.m_Name != m_Name )
        return false;
    if( rhs.m_Address != m_Address )
        return false;
    return true;
}

bool AmigaHost::hasTimedOut()
{
    QTime now = QTime::currentTime();
    if( m_TimeoutTime < now )
        return true;

    return false;
}

void AmigaHost::setHostRespondedNow()
{
    //Quickly check the settings for a timeout value
    m_Settings->beginGroup( SETTINGS_GENERAL );
    int timeoutPeriod = m_Settings->value( SETTINGS_HELLO_TIMEOUT, 10 ).toInt();
    m_Settings->endGroup();

    m_TimeoutTime = QTime::currentTime().addSecs( timeoutPeriod );
}

const QString &AmigaHost::Name() const
{
    return m_Name;
}

const QString &AmigaHost::OsName() const
{
    return m_OsName;
}

const QString &AmigaHost::OsVersion() const
{
    return m_OsVersion;
}

const QString &AmigaHost::Hardware() const
{
    return m_Hardware;
}

const QHostAddress &AmigaHost::Address() const
{
    return m_Address;
}
