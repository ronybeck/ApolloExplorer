#include "hostlister.h"
#include "AEUtils.h"


HostLister::HostLister( QSharedPointer<QSettings> settings, QObject *parent )
    : QObject{parent},
      m_Settings( settings ),
      m_DeviceDiscovery( new DeviceDiscovery( settings, this ) ),
      m_HelloTimeout( 5 ),
      m_TimeoutTimer( new QTimer( this ) )
{
    //get the HelloTimeout from the settings
    m_Settings->beginGroup( SETTINGS_GENERAL );
    m_HelloTimeout = m_Settings->value( SETTINGS_HELLO_TIMEOUT, 2 ).toInt();
    m_Settings->endGroup();

    //Signal Slots
    connect( m_DeviceDiscovery, &DeviceDiscovery::hostAliveSignal, this, &HostLister::onNewDeviceDiscoveredSlot );
    connect( m_DeviceDiscovery, &DeviceDiscovery::hostDiedSignal, this, &HostLister::onDeviceLeftSlot );

    //Setup out timeout timer
    connect( m_TimeoutTimer, &QTimer::timeout, this, &HostLister::onScanningTimeoutSlot );
    m_TimeoutTimer->setSingleShot( true );
    m_TimeoutTimer->start( m_HelloTimeout * 1000 );
}

HostLister::~HostLister()
{
    //clear out the host list
    m_HostList.clear();

    //Stop and destroy the timer
    m_TimeoutTimer->stop();
    disconnect( m_TimeoutTimer );
    delete m_TimeoutTimer;

    //Delete the device discoverer
    delete m_DeviceDiscovery;
}

QStringList HostLister::getHostLike( QString pattern )
{
    QStringList candidates;

    //Go through the list of hosts and see what hosts start with the patter
    QVector<QSharedPointer<AmigaHost>>::Iterator iter;
    for( iter = m_HostList.begin(); iter != m_HostList.end(); iter++ )
    {
        QSharedPointer<AmigaHost> host = *iter;

        //Does it match a name?
        if( host->Name().startsWith( pattern, Qt::CaseInsensitive ) )
        {
            candidates.append( host->Name() + " " + host->Address().toString() );
        }

        //Does it match an IP Address
        if( host->Address().toString().startsWith( pattern, Qt::CaseInsensitive ) )
        {
            candidates.append( host->Name() + " " + host->Address().toString()  );
        }
    }

    //Return what we found
    return candidates;
}

void HostLister::onNewDeviceDiscoveredSlot( QSharedPointer<AmigaHost> host )
{
    m_HostList.append( host );
    DBGLOG << "Host " << host->Name() << "(" << host->Address().toIPv4Address() << ") found.";
}

void HostLister::onDeviceLeftSlot( QSharedPointer<AmigaHost> host )
{
    m_HostList.remove( m_HostList.indexOf( host ) );
    DBGLOG << "Host " << host->Name() << "(" << host->Address().toIPv4Address() << ") removed.";
}

void HostLister::onScanningTimeoutSlot()
{
    emit hostsDiscoveredSignal( m_HostList );
}

