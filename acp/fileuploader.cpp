#include "fileuploader.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <iostream>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>

#define DEBUG 1
#include "AEUtils.h"

FileUploader::FileUploader( QStringList localSources, QString remoteDestination, QString remoteHost, QObject *parent )
    : QObject{parent},
      m_SourcePaths( localSources ),
      m_DestinationPath( remoteDestination ),
      m_DestinationHost( remoteHost ),
      m_UploadThread( new UploadThread( this ) ),
    m_FileList( ),
    m_CurrentLocalPath( "" ),
    m_CurrentRemotePath( "" ),
    m_Recursive( false ),
    m_DisconnectRequested( false )
{

    //Connect this object with the uploader thread
    connect( this, &FileUploader::createDirectorySignal, m_UploadThread.get(), &UploadThread::onCreateDirectorySlot );
    connect( this, &FileUploader::startFileSignal, m_UploadThread.get(), &UploadThread::onStartFileSlot );
    connect( m_UploadThread.get(), &UploadThread::directoryCreationCompletedSignal, this, &FileUploader::directoryCreationCompletesSlot );
    connect( m_UploadThread.get(), &UploadThread::directoryCreationFailedSignal, this, &FileUploader::directoryCreationFailedSlot );
    connect( m_UploadThread.get(), &UploadThread::uploadCompletedSignal, this, &FileUploader::fileUploadCompletedSlot );
    connect( m_UploadThread.get(), &UploadThread::uploadFailedSignal, this, &FileUploader::fileUploadFailedSlot );
    connect( m_UploadThread.get(), &UploadThread::disconnectFromHostSignal, this, &FileUploader::disconnectedFromServerSlot );
    connect( m_UploadThread.get(), &UploadThread::connectedToServerSignal, this, &FileUploader::connectedToServerSlot );
    connect( m_UploadThread.get(), &UploadThread::uploadProgressSignal, this, &FileUploader::uploadProgressSlot );

    m_UploadThread->start();
    QThread::msleep(200);
}

FileUploader::~FileUploader()
{
    m_UploadThread->stopThread();
    m_UploadThread->wait();
}

bool FileUploader::connectToHost()
{
    //We will want to wait for the connection to happen. So we need some tracking for that
    bool connectedToHost = false;
    bool disconnectedFromHost = false;

    connect( m_UploadThread.get(), &UploadThread::connectedToServerSignal, [&]()
    {
        connectedToHost = true;
    });
    connect( m_UploadThread.get(), &UploadThread::disconnectedFromServerSignal,[&]( )
    {
        disconnectedFromHost = true;
    });

    //Connect to the remote host
    DBGLOG << "Connecting to server" << m_DestinationHost.toString();
    m_UploadThread->onConnectToHostSlot( m_DestinationHost, MAIN_LISTEN_PORTNUMBER );

    //Wait for the connection to happen
    for( int i = 0; i < 1000; i++ )
    {
        if( connectedToHost ) break;
        if( disconnectedFromHost ) break;
        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    //Did we connect?
    if( !connectedToHost )
    {
        DBGLOG << "Connection failed it seems";
        return false;
    }

    //It seems beneficial to put a wait here. Seems to fix a crash
    QThread::msleep( 500 );

    return true;
}

void FileUploader::disconnectFromHost()
{
    //This is a requested disconnect so we don't want to fire alarms
    m_DisconnectRequested = true;

    //We will want to wait for the connection to happen. So we need some tracking for that
    bool connectedToHost = true;

    connect( m_UploadThread.get(), &UploadThread::disconnectFromHostSignal, [&]( )
    {
        connectedToHost = false;
    });

    //Connect to the remote host
    m_UploadThread->onDisconnectFromHostRequestedSlot();

    //Wait for the connection to happen
    for( int i = 0; i < 1000; i++ )
    {
        if( !connectedToHost )   break;
        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    return;
}

static QMap<QString,QString> populateFromDirectory( QString localPath, QString remotePath )
{
    QMap<QString,QString> mappings;
    QString destinationPath = remotePath;

    //Do some formating on the remote path to make sure it has all the drive demarkations
    if( !destinationPath.contains( ":" ) )
    {
        //remove erroneous "/" at the end
        if( destinationPath.endsWith( "/" ) )   destinationPath.chop( 1 );
        destinationPath += ":";
    }

    //Get the details of the local directory
    QFile localFile( localPath );
    QFileInfo localFileInfo( localFile );
    QDir localFileDir( localPath );

    //Get the directory contents
    QStringList entries = localFileDir.entryList();

    //Go through each and add them to the mappings
    QStringListIterator iter( entries );
    while( iter.hasNext() )
    {
        //Get the path of the next file
        QString filename = iter.next();

        //We need to ignore special paths like "." and ".."
        if( !filename.compare( "." ) || !filename.compare( ".." ) ) continue;

        //Build the full path to the entry and get the file info
        QString subPath = localPath + "/" + filename;
        QFileInfo subFileInfo( subPath );

        //Workout the final destination path for the file
        QString finalDestinationPath = destinationPath;
        if( !finalDestinationPath.endsWith( ":" ) ) finalDestinationPath += "/";
        finalDestinationPath.append( filename );

        //If this is a file, we can map immediately
        if( subFileInfo.isFile() )
        {
            DBGLOG << "Adding file " << subPath << " <----> " << finalDestinationPath;
            mappings[ subPath ] = finalDestinationPath;
        }

        //If it is a directory
        if( subFileInfo.isDir() )
        {
            //Add this directoy
            DBGLOG << "Adding dir " << subPath << " <----> " << finalDestinationPath;
            mappings[ subPath ] = finalDestinationPath;

            //Recurse into this directory
            mappings.insert( populateFromDirectory( subPath, finalDestinationPath ) );
        }
    }

    //Now give back what we found
    return mappings;
}

bool FileUploader::startUpload()
{   
    //We need to go through the local file list and produce a mapping of localfile<--->remotefile
    for( auto& nextPath: m_SourcePaths )
    {
        //Remove the trailing "/" so QFileInfo will give us a basename
        if( nextPath.endsWith( "/" ) ) nextPath = nextPath.chopped(1);

        //build up the source and destination paths
        QFile localPath( nextPath );
        QFileInfo localPathInfo( localPath );
        QString localFilePath = nextPath;
        QString remoteFilePath = m_DestinationPath;

        if( !localPath.exists() )
        {
            std::cerr << "Path " << localPathInfo.path().toStdString() << " does not exist " << std::endl;
            return false;
        }

        //Format the remote file path
        if( !remoteFilePath.contains( ":" ) )
        {
            if( remoteFilePath.contains( "/" ) )
            {
                //Assume the first "/" is where the ":" should got to designate the drive
                remoteFilePath.replace( remoteFilePath.indexOf( "/" ), 1, ":" );
            }else
            {
                //With no drive designator or directory designator, we have no choice but to assume this is a drive
                remoteFilePath.append( ":" );
            }
        }


        //Now append the file name
        if( !remoteFilePath.endsWith( ":" ) && !remoteFilePath.endsWith( "/" ) )
            remoteFilePath.append( "/" );
        remoteFilePath.append( localPathInfo.completeBaseName() );  //Just append the file name

        //TODO: What happens if someone wants to upload '*'?

        //Is this a simple filename?  If so, this is easy
        if( localPathInfo.isFile() )
        {
            //Add this to the list
            m_FileList[ localFilePath ] = remoteFilePath;

        }else if( localPathInfo.isDir() ) //If this is a directory then we need to drill down through the list and add each item
        {
            //we will do directories only if we are be recursive
            if( m_Recursive )
            {
                //Add this to the list
                m_FileList[ localFilePath ] = remoteFilePath;

                //Now get the contents of the directory
                m_FileList.insert( populateFromDirectory( localFilePath, remoteFilePath ) );
            }
        }else
        {
            std::cerr << "Not sure what path " << localPathInfo.absoluteFilePath().toStdString() << " is.";
            return false;
        }
    }

    //Now we can start the first file upload by calling.....
    fileUploadCompletedSlot();

    return true;
}

void FileUploader::setRecursive(bool enabled)
{
    m_Recursive = enabled;
}

void FileUploader::fileUploadCompletedSlot()
{
    //Do we have any files left?
    if( m_FileList.size() == 0 )
    {
        //Go to the next line on the console now
        std::cout << std::endl;

        emit uploadCompletedSignal();
        return;
    }

    //Ok, get the next file from the list
    m_CurrentLocalPath = m_FileList.firstKey();
    m_CurrentRemotePath = m_FileList.take( m_CurrentLocalPath );

    //What is this that we are uploading?  A file or a directory
    QFileInfo fileInfo( m_CurrentLocalPath );
    if( fileInfo.isFile() )
    {
        //Go to the next line on the console now
        std::cout << std::endl;

        //Now emit the signal to start the upload
        emit startFileSignal( m_CurrentLocalPath, m_CurrentRemotePath );
    }
    else if( fileInfo.isDir() )
    {
        //Emit a signal to create a directory
        emit createDirectorySignal( m_CurrentRemotePath );
    }
}

void FileUploader::fileUploadFailedSlot(UploadThread::UploadFailureType failure)
{
    //give up?
    emit  uploadAbortedSignal( "Failed to upload file" );
}

void FileUploader::uploadProgressSlot(quint8 percent, quint64 progressBytes, quint64 throughput)
{
    int consoleWidth = 40;
#if __linux__
    struct winsize w;
    ioctl( STDOUT_FILENO, TIOCGWINSZ, &w );
    if( w.ws_col/2 > consoleWidth)
        consoleWidth = w.ws_col/2;
#endif

    //Stick to a maximum length of 30 characters
    QString displayName = m_CurrentLocalPath;
    if( displayName.length() > consoleWidth )
    {
        displayName = m_CurrentLocalPath.right( consoleWidth );
        displayName.replace( 0,3,"..." );
    }
    while( displayName.length() < consoleWidth )
        displayName.append( " " );

    //Print the file name
    std::cout << "\r" << displayName.toStdString() << " ";

    //Print the progress bar
    std::cout << "[";
    for( int i = 0; i < percent/5; i++ )    std::cout << "=";
    for( int i = percent/5; i < 20; i++ )  std::cout << " ";
    std::cout << "] ";

    //Print number of bytes sent
    std::cout << progressBytes/1024 << "KB ";

    //Print the current speed
    std::cout << throughput/1024 << "KB/s";
}

void FileUploader::connectedToServerSlot()
{
    DBGLOG << "Connected.";
}

void FileUploader::disconnectedFromServerSlot()
{
    //Did the server throw us out or did we disconenct ourselves?
    if( !m_DisconnectRequested )
        emit  uploadAbortedSignal( "Disconnected from server" );
}

void FileUploader::directoryCreationCompletesSlot()
{
    DBGLOG << "Directory created";
    fileUploadCompletedSlot();
}

void FileUploader::directoryCreationFailedSlot()
{
    //give up?
    DBGLOG << "Directory creation failed.";
    emit  uploadAbortedSignal( "Failed to create directory" );
}
