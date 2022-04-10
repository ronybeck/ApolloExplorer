#include "devicediscovery.h"
#include <QtEndian>

#define LOCK QMutexLocker locker( &m_Mutex )
#define UNLOCK locker.unlock()
#define RELOCK locker.lock()

DeviceDiscovery::DeviceDiscovery(QObject *parent) :
    QObject(parent),
    m_ScanTimer( ),
    m_Socket( ),
    m_HostList( ),
    m_Mutex( QMutex::Recursive )
{
    //Signal SLots
    connect( &m_ScanTimer, &QTimer::timeout, this, &DeviceDiscovery::onScanTimerTimeoutSlot );
    connect( &m_Socket, &QUdpSocket::readyRead, this, &DeviceDiscovery::onSocketReadReadySlot );

    //Setup the socket
    m_Socket.bind( QHostAddress::AnyIPv4, BROADCAST_PORTNUMBER );
    m_Socket.open( QIODevice::ReadWrite );

    //Setup the timer
    m_ScanTimer.setInterval( 2000 );
    m_ScanTimer.setSingleShot( false );
    m_ScanTimer.start();

    //Trigger a scan now
    onScanTimerTimeoutSlot();
}


void DeviceDiscovery::onScanTimerTimeoutSlot()
{

    //Form the message we will send to all clients
    ProtocolMessage_DeviceDiscovery_t message;
    message.header.token = qToBigEndian<quint32>( MAGIC_TOKEN );
    message.header.length = qToBigEndian<quint32>( sizeof( message ) );
    message.header.type = qToBigEndian<quint32>( PMT_DEVICE_DISCOVERY );

    QByteArray payload( reinterpret_cast<char*>( &message ), sizeof( message ) );
    m_Socket.writeDatagram( payload, QHostAddress( "255.255.255.255" ), BROADCAST_PORTNUMBER );

    //Check if any of the hosts have timed out
    LOCK;
    QMapIterator<QString, QSharedPointer<VNetHost>> iter( m_HostList );
    while( iter.hasNext() )
    {
        auto entry = iter.next();
        QSharedPointer<VNetHost> host = entry.value();
        QString address = entry.key();
        if( host->hasTimedOut() )
        {
            m_HostList.remove( address );

            //Emit the leave signal
            qDebug() << "Server " << host->Name() << " (OS: " << host->OsName() << " ver: " << host->OsVersion() << ") is offline.";
            emit hostDiedSignal( host );
        }
    }
}


void DeviceDiscovery::onSocketReadReadySlot()
{
    ProtocolMessage_DeviceAnnouncement_t message;
    QHostAddress sender;


    //Get the arriving datagram
    quint64 bytesRead = m_Socket.readDatagram( reinterpret_cast< char* >( &message ), sizeof( message ), &sender );

    //endian conversion
    message.header.token = qFromBigEndian<quint32>( message.header.token );
    message.header.type = qFromBigEndian<quint32>( message.header.type );
    message.header.length = qFromBigEndian<quint32>( message.header.length );

    //It needs to be the right size
    if( bytesRead != sizeof( message ) )
    {
       //qDebug() << "Ignoring datagrame of size " << bytesRead << " from host " << sender;
        return;
    }

    //It also needs to have a token
    if( message.header.token != MAGIC_TOKEN )
    {
        qDebug() << "Ignoring datagram with token " << QByteArray( reinterpret_cast< char* >( &message.header.token ), 4 ).toHex() << " from host " << sender;
        return;
    }

    //The type needs to be right as well
    //It also needs to have a token
    if( message.header.type != PMT_DEVICE_ANNOUNCE )
    {
        qDebug() << "Ignoring datagram with type " << QByteArray( reinterpret_cast< char* >( message.header.type ) ) << " from host " << sender;
        return;
    }

    //Otherwise we are good to go
    QString name( message.name );
    QString osName( message.osName );
    QString osVersion( message.osVersion );
    QString hardware( message.hardware );

    LOCK;

    //Does this IP Address exist in the database already?
    QString senderAddressString = sender.toString();
    if( !m_HostList.contains( senderAddressString ) )
    {
        //add this to the list
        VNetHost *host = new VNetHost( name, osName, osVersion, hardware, sender, this );
        m_HostList[ senderAddressString ] = QSharedPointer<VNetHost>( host );

        //Tell the world about this
        qDebug() << "Server " << name << " (OS: " << osName << " ver: " << osVersion << ") is online.";
        emit hostAliveSignal( m_HostList[ senderAddressString ] );

        return;
    }

    //If this already exists, just update the timestampe
    m_HostList[ senderAddressString ]->setHostRespondedNow();
}
