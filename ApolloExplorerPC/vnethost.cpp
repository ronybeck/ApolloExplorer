#include "vnethost.h"

VNetHost::VNetHost( QString name, QString osName, QString osVersion, QString hardware, QHostAddress address, QObject *parent ) :
    QObject(parent),
    m_Name( name ),
    m_OsName( osName ),
    m_OsVersion( osVersion ),
    m_Hardware( hardware ),
    m_Address( address )
{
    m_TimeoutTime = QTime::currentTime().addSecs( 10 );
}

bool VNetHost::operator ==( const VNetHost &rhs )
{
    if( rhs.m_Name != m_Name )
        return false;
    if( rhs.m_Address != m_Address )
        return false;
    return true;
}

bool VNetHost::hasTimedOut()
{
    QTime now = QTime::currentTime();
    if( m_TimeoutTime < now )
        return true;

    return false;
}

void VNetHost::setHostRespondedNow()
{
    m_TimeoutTime = QTime::currentTime().addSecs( 4 );
}

const QString &VNetHost::Name() const
{
    return m_Name;
}

const QString &VNetHost::OsName() const
{
    return m_OsName;
}

const QString &VNetHost::OsVersion() const
{
    return m_OsVersion;
}

const QString &VNetHost::Hardware() const
{
    return m_Hardware;
}

const QHostAddress &VNetHost::Address() const
{
    return m_Address;
}
