#include "directorylister.h"

#include <iostream>
#include <QThread>
#include <QCoreApplication>

#define DEBUG1
#include "AEUtils.h"

DirectoryLister::DirectoryLister( QString ipAddressString, QString path, QObject *parent )
    : QObject{parent},
      m_Host( ipAddressString ),
      m_Path( path ),
      m_ProtocolHander( this )
{
    //As soon as we are connected, we will receive the volume list.  We need a slot for that
    connect( &m_ProtocolHander, &ProtocolHandler::volumeListSignal, this, &DirectoryLister::onVolumeListSlot );
}

QStringList DirectoryLister::getDirectoryList()
{
    QStringList dirContents;

    //Connect to the host
    DBGLOG << "Connecting to host";
    m_ProtocolHander.onConnectToHostRequestedSlot( QHostAddress( m_Host ), MAIN_LISTEN_PORTNUMBER );


    //Wait for the connect to happen
    bool connected = false;
    bool connectionFailed = false;
    QObject::connect( &m_ProtocolHander, &ProtocolHandler::serverVersionSignal, [&]( quint8 major, quint8 minor, quint8 revision ) {
        Q_UNUSED( major )
        Q_UNUSED( minor )
        Q_UNUSED( revision )
        connected = true;
    });

    QObject::connect( &m_ProtocolHander, &ProtocolHandler::disconnectFromHostSignal, [&]() {
        connectionFailed = true;
    });

    //Perform the wait on the
    for( int i=0; i< 1000; i++)
    {
        if( connected ) break;
        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    //Did we connected?
    if( !connected )
    {
        std::cout << "Couldn't connect to host." << std::endl;
        return dirContents;
    }

    DBGLOG << "Connected.";

    //So now we are connected
    //Get the directory in question
    if( m_Path.length() == 0 )
    {
        //Ok, no path was specified, so get the drive list
        m_ProtocolHander.onGetVolumeListSlot();

        //If we don't have the drive list, lets cycle a few times and see what we get
        for( int i=0; i < 1000; i++ )
        {
            //Do we have the list of drives already?
            if( m_Volumes.size() > 0 )
            {
                //Ok we have the list of drives.
                //Lets reproduce this as strings
                QListIterator<QSharedPointer<DiskVolume>> iter( m_Volumes );
                while( iter.hasNext() )
                {
                    QSharedPointer<DiskVolume> nextVolume = iter.next();

                    dirContents.append( nextVolume->getName() + ":" );
                }

                //Now return the list
                return dirContents;
            }

            //Wait a bit
            QThread::msleep( 10 );
            QCoreApplication::processEvents();
        }
    }

    //Check the user supplied path
    m_ProtocolHander.onGetDirectorySlot( m_Path );
    //Wait for the hostlist signal to come
    bool retrivalCompleted = false;
    QSharedPointer<DirectoryListing> newListing;
    QVector<QSharedPointer<AmigaHost>> hostList;
    QObject::connect( &m_ProtocolHander, &ProtocolHandler::newDirectoryListingSignal, [&]( QSharedPointer<DirectoryListing> listing ) {
        retrivalCompleted = true;
        newListing = listing;
    });

    //Perform the wait
    while( !retrivalCompleted )
    {
        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    //Now process the list we got
    for( auto iter = newListing->Entries().begin(); iter != newListing->Entries().end(); iter++ )
    {
        QSharedPointer<DirectoryListing> entry = *iter;
        QString entryPath = entry->Path();
        if( entry->isDirectory() ) entryPath += "/";
        dirContents.append( entryPath );
    }

    return dirContents;
}

void DirectoryLister::onVolumeListSlot(QList<QSharedPointer<DiskVolume> > volumes)
{
    m_Volumes = volumes;
}
