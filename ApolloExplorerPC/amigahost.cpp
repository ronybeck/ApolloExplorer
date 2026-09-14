#include "amigahost.h"
#include "qpixmap.h"

AmigaHost::AmigaHost( quint32 timeoutInSeconds, QString name, QString osName, QString osVersion, HardwareType hardware, QHostAddress address, bool isStaticallyConfigured, QObject *parent ) :
    QObject(parent),
    m_Name( name ),
    m_OsName( osName ),
    m_OsVersion( osVersion ),
    m_Hardware( hardware ),
    m_Address( address ),
    m_TimeoutInSeconds( timeoutInSeconds ),
    m_StaticlyConfiguredHost( isStaticallyConfigured )
{
    m_TimeoutTime = QTime::currentTime().addSecs( timeoutInSeconds + 20 );
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
    m_TimeoutTime = QTime::currentTime().addSecs( m_TimeoutInSeconds );
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

const QString AmigaHost::HardwareName() {
    return AmigaHost::hardwareTypeAsString( m_Hardware );
}

const AmigaHost::HardwareType &AmigaHost::Hardware() const
{
    return m_Hardware;
}

const QHostAddress &AmigaHost::Address() const
{
    return m_Address;
}


QPixmap AmigaHost::getPixmap( HardwareType &type )
{
    switch ( type ) {
        case HARDWARE_TYPE_V4:
        case HARDWARE_TYPE_V500:
        case HARDWARE_TYPE_V600:
        case HARDWARE_TYPE_V1200:
            return QPixmap( ":/browser/icons/VampireHW.png" );
            break;
        case HARDWARE_TYPE_FIREBIRD:
            return QPixmap( ":/browser/icons/FireBirdHW.png" );
            break;
        case HARDWARE_TYPE_MANTICORE:
            return QPixmap( ":/browser/icons/MantiCoreHW.png" );
            break;
        case HARDWARE_TYPE_ICEDRAKE:
            return QPixmap( ":/browser/icons/IceDrakeHW.png" );
            break;
        case HARDWARE_TYPE_UNICORN:
            return QPixmap( ":/browser/icons/UniCornHW.png" );
            break;
        default:
            return QPixmap( ":/browser/icons/CommodoreHW.png" );
            break;
    }

    return QPixmap( ":/browser/icons/CommodoreHW.png" );
}

QString AmigaHost::hardwareTypeAsString( HardwareType &type ) {
    QString hardwareName = "Amiga";

    switch ( type ) {
    case HARDWARE_TYPE_V4:
        return QString( "V4-SA (StandAlone)" );
        break;
    case HARDWARE_TYPE_V500:
        return QString( "V2-A500" );
        break;
    case HARDWARE_TYPE_V600:
        return QString( "V2-A600" );
        break;
    case HARDWARE_TYPE_V1200:
        return QString( "V2-A1200" );
        break;
    case HARDWARE_TYPE_FIREBIRD:
        return QString( "V4-A500 (FireBird)" );
        break;
    case HARDWARE_TYPE_MANTICORE:
        return QString( "V4-A600 (MantiCore)" );
        break;
    case HARDWARE_TYPE_ICEDRAKE:
        return QString( "V4-A1200 (IceDrake)" );
        break;
    case HARDWARE_TYPE_UNICORN:
        return QString( "V4-A6000 (UniCorn)" );
        break;
    default:
        return QString( "Amiga" );
        break;
    }

    return hardwareName;
}

bool AmigaHost::StaticlyConfiguredHost() const
{
    return m_StaticlyConfiguredHost;
}

AmigaHost::HardwareType AmigaHost::hardwareTypeFromString(QString &name)
{
    if ( !name.compare( "V4-SA (StandAlone)" ) ) { return HARDWARE_TYPE_V4; }
    if ( !name.compare( "V2-A500" ) ) { return HARDWARE_TYPE_V500; }
    if ( !name.compare( "V2-A600" ) ) { return HARDWARE_TYPE_V600; }
    if ( !name.compare( "V2-A1200" ) ) { return HARDWARE_TYPE_V1200; }
    if ( !name.compare( "V4-A500 (FireBird)" ) ) { return HARDWARE_TYPE_FIREBIRD; }
    if ( !name.compare( "V4-A600 (MantiCore)" ) ) { return HARDWARE_TYPE_MANTICORE; }
    if ( !name.compare( "V4-A1200 (IceDrake)" ) ) { return HARDWARE_TYPE_ICEDRAKE; }
    if ( !name.compare( "V4-A6000 (UniCorn)" ) ) { return HARDWARE_TYPE_UNICORN; }

    return HARDWARE_TYPE_UNKNOWN;
}
