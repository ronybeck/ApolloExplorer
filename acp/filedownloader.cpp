#include "filedownloader.h"

#define DEBUG 0
#include "AEUtils.h"

#include <QCoreApplication>
#include <QDir>
#include <QRegExp>
#include <iostream>
#include <stdlib.h>
#if __linux__ || __APPLE__
#include <sys/ioctl.h>
#endif
#include <stdio.h>
#include <unistd.h>

FileDownloader::FileDownloader( QString remoteSources, QString localDestination, QString remoteHost, QObject *parent  )
    : QObject{parent},
    m_DownloadThread( new DownloadThread( this ) ),
    m_RemoteSource( remoteSources ),
    m_RemoteSourceParent ( "" ),
    m_LocalDestination( localDestination ),
    m_RemoteHost( remoteHost ),
    m_RemoteDirectoriesToProcess( ),
    m_DownloadList( ),
    m_Recusive( false ),
    m_CurrentRemotePath( "" ),
    m_CurrentLocalPath( "" ),
    m_FileMatchPattern( "" )
{
    //Setup the signals/slots
    connect( this, &FileDownloader::startDownloadSignal, m_DownloadThread.get(), &DownloadThread::onStartFileSlot );
    connect( m_DownloadThread.get(), &DownloadThread::downloadProgressSignal, this, &FileDownloader::ondownloadProgressSlot );
    connect( m_DownloadThread.get(), &DownloadThread::downloadCompletedSignal, this, &FileDownloader::onDownloadCompletedSlot );
    connect( m_DownloadThread.get(), &DownloadThread::operationTimedOutSignal, this, &FileDownloader::onOperationTimedoutSlot );

    //If the local path is specified as "." or some other relative path, we should try and detirmine the absolute path
    QDir destinationDir( m_LocalDestination );
    m_LocalDestination = destinationDir.absolutePath() + "/";
    DBGLOG << "Downloading files to " << m_LocalDestination;

    //Start the download thread
    m_DownloadThread->start();
    QThread::msleep( 500 );
}

FileDownloader::~FileDownloader()
{
    m_DownloadThread->stopThread();
    m_DownloadThread->wait();
}

bool FileDownloader::connectToHost()
{
    //We will want to wait for the connection to happen. So we need some tracking for that
    bool connectedToHost = false;
    bool disconnectedFromHost = false;

    connect( m_DownloadThread.get(), &DownloadThread::connectedToServerSignal, [&]()
            {
                connectedToHost = true;
            });
    connect( m_DownloadThread.get(), &DownloadThread::disconnectedFromServerSignal,[&]( )
            {
                disconnectedFromHost = true;
            });

    //Connect to the remote host
    DBGLOG << "Connecting to server" << m_RemoteHost.toString();
    m_DownloadThread->onConnectToHostSlot( m_RemoteHost, MAIN_LISTEN_PORTNUMBER );

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

void FileDownloader::disconnectFromHost()
{
    //We will want to wait for the connection to happen. So we need some tracking for that
    bool connectedToHost = true;

    connect( m_DownloadThread.get(), &DownloadThread::disconnectFromHostSignal, [&]( )
            {
                connectedToHost = false;
            });

    //Connect to the remote host
    m_DownloadThread->onDisconnectFromHostRequestedSlot();

    //Wait for the connection to happen
    for( int i = 0; i < 1000; i++ )
    {
        if( !connectedToHost )   break;
        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    return;
}

bool FileDownloader::startDownload()
{
    //Set the starting point for the download
    m_CurrentLocalPath = m_LocalDestination;
    m_CurrentRemotePath = m_RemoteSource;
    bool remotePathIsWildCard = false;

    //Does the local destination exist?
    QDir localDestinationDir( m_LocalDestination );
    if( !localDestinationDir.exists() )
    {
        std::cerr << "The local path " << m_LocalDestination.toStdString() << " doesn't exist" << std::endl;
        return false;
    }

    //We need to reformat the remote path to an amiga style DRIVE:DIR/DIR/FILE.EXT
    //Do we have any "/" characters?  If not, this is a drive we are trying to download
    if( !m_CurrentRemotePath.contains( "/" ) && !m_CurrentRemotePath.contains( ":" ) )
    {
        //Just append a ":"
        m_CurrentRemotePath.append( ":" );
    }

    //First replace the first "/" with a ":"....so long as there isn't already one.
    if( !m_CurrentRemotePath.contains( ":" ) )
    {
        m_CurrentRemotePath.replace( m_CurrentRemotePath.indexOf( '/' ), 1, ":" );
    }

    //We need to sanitise the remote path a little before requesting a directory listing from the server
    //If patterns or wild cards are used, we will need to get the root path first
    QString pathToGet;
    QString lastElement;
    if( !m_CurrentRemotePath.contains( "/" ) )
    {
        lastElement = m_CurrentRemotePath.right( m_CurrentRemotePath.length() - m_CurrentRemotePath.indexOf( ":" ) - 1 );
        pathToGet = m_CurrentRemotePath.left( m_CurrentRemotePath.indexOf( ":" ) + 1 );
    }else
    {
        lastElement = m_CurrentRemotePath.right( m_CurrentRemotePath.length() - m_CurrentRemotePath.lastIndexOf( "/" ) - 1 );
        pathToGet = m_CurrentRemotePath.left( m_CurrentRemotePath.lastIndexOf( "/" ) + 1);
    }

    //Now setup the pattern matching.  If we have a precise file name, that is the pattern we will use.
    //If we don't have a filename or a wildcard, we will assume a wildcard (because the user obviously wanted the whole directory).
    m_FileMatchPattern = lastElement;
    if( lastElement.contains( "*" ) || lastElement.contains( "#?" ) )
    {
        //Ok we have a search pattern
        m_FileMatchPattern.replace( "#?", "*" );

        //Mark this as a wildcard path
        remotePathIsWildCard = true;
    }else if( lastElement.length() == 0 )
    {
        m_FileMatchPattern = "*";
    }

    //We should use this pathToGet as the parent path for relative path calculations later
    m_RemoteSourceParent = pathToGet;

    DBGLOG << "Path to get: " << pathToGet;
    DBGLOG << "File Match Pattern: " << m_FileMatchPattern;

    //Now get the path in question
    //TODO: What happens when we have a timeout here?
    QSharedPointer<DirectoryListing> listing = m_DownloadThread->onGetDirectoryListingSlot( pathToGet );
    if( listing.isNull() )
    {
        emit downloadAbortedSignal( "The path couldn't be found" );
        return false;
    }

    //Setup the regex for wildcard matching
    QRegExp wildcardExp( m_FileMatchPattern );
    wildcardExp.setPatternSyntax( QRegExp::Wildcard );

    //Add all the files we have to the list
    for( auto entry: listing->Entries() )
    {
        DBGLOG << "Examining path " << entry->Path();

        if( m_FileMatchPattern.length() )
        {
            if( !wildcardExp.exactMatch( entry->Name() ) )
            {
                DBGLOG << "File " << entry->Path() << " doesn't match pattern " << m_FileMatchPattern;
                continue;
            }
        }

        if( entry->isDirectory() )
        {
            //Are we recursively searching for files?
            if( m_Recusive )
            {
                //Add a directory to the list
                m_RemoteDirectoriesToProcess.append( entry->Path() );
            }
        }else
        {
            QString localFilePath = m_LocalDestination + "/" + entry->Name();
            m_DownloadList[ localFilePath ] = entry;
        }
    }

    //Did we find something to download?
    if( m_DownloadList.isEmpty() && m_RemoteDirectoriesToProcess.isEmpty() )
    {
        std::cerr << "No files of directories found" << std::endl;
        return false;
    }

    //Now call the onDownloadCompleteSlot to start the next file
    onDownloadCompletedSlot();

    return true;
}

void FileDownloader::setRecursive( bool enabled )
{
    m_Recusive = enabled;
}

void FileDownloader::onDownloadCompletedSlot()
{
    //Jump to the next line on the console
    std::cout << std::endl;

    //Is the file download queue empty?
    while( m_DownloadList.empty() && !m_RemoteDirectoriesToProcess.empty() )
    {
        //Now process the next directory while ever we still have stuff to process
        while( !getNextDirectory() && !m_RemoteDirectoriesToProcess.empty() ) {}

        //Are we out of things to do?
        if( m_RemoteDirectoriesToProcess.empty() && m_DownloadList.empty() )
        {
            //I guess we are finished here
            emit downloadCompletedSignal();
            return;
        }
    }

    //One last check
    if( m_DownloadList.empty() )
    {
        //I guess we are finished here
        emit downloadCompletedSignal();
        return;
    }

    //Otherwise, we should get the next file in the list
    QString localPath = m_DownloadList.firstKey();
    QString remotePath = m_DownloadList[ localPath ]->Path();
    m_CurrentLocalPath = localPath;
    m_CurrentRemotePath = remotePath;
    m_DownloadList.remove( localPath );
    DBGLOG << "Downloading " << remotePath << " to " << localPath;
    emit startDownloadSignal( localPath, remotePath );
}

void FileDownloader::ondownloadProgressSlot( quint8 percent, quint64 progressBytes, quint64 throughput )
{
    int consoleWidth = 30;
#if __linux__ || __APPLE__
    struct winsize w;
    ioctl( STDOUT_FILENO, TIOCGWINSZ, &w );
    if( w.ws_col/2 > consoleWidth)
        consoleWidth = w.ws_col/2;
#endif

    //Stick to a maximum length of 30 characters
    QString displayName = m_CurrentRemotePath;
    if( displayName.length() > consoleWidth )
    {
        displayName = m_CurrentRemotePath.right( consoleWidth );
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

void FileDownloader::onDisconnectedFromServerSlot()
{

}

void FileDownloader::onConnectedToServerSlot()
{

}

void FileDownloader::onDirectoryListingSlot( QSharedPointer<DirectoryListing> listing )
{

}

void FileDownloader::onAbortSlot(QString reason)
{
    std::cerr << "The operation was aborted: " << reason.toStdString() << std::endl;
    emit downloadCompletedSignal();
}

void FileDownloader::onOperationTimedoutSlot()
{
    std::cout << "Operation timed out." << std::endl << "Reconnecting." << std::endl;

    //Try to reconnect
    disconnectFromHost();
    if( !connectToHost() )
    {
        std::cerr << "Failed to reconnect to host" << std::endl;
        emit downloadCompletedSignal();
        emit downloadAbortedSignal( "Failed to reconnet to host." );
        return;
    }

    //A short delay between connecting and sending the next command seems to help
    QThread::msleep( 500 );

    //Now that we are connected, we should start the download again.
    DBGLOG << "Downloading " << m_CurrentRemotePath << " to " << m_CurrentLocalPath;
    emit startDownloadSignal( m_CurrentLocalPath, m_CurrentRemotePath );
}

bool FileDownloader::getNextDirectory()
{
    //Are we out of directories to process?
    if( m_RemoteDirectoriesToProcess.empty() )  return false;

    //Get the next directory from the list
    QString remotePath = m_RemoteDirectoriesToProcess.front();
    m_RemoteDirectoriesToProcess.pop_front();
    DBGLOG << "Getting directory " << remotePath;

    //Workout what the relative local path would be now based on this paths relative location to the original path.
    QString relativeRemotePath = remotePath;
    relativeRemotePath.remove( m_RemoteSourceParent );
    QString localParentDir = m_LocalDestination + relativeRemotePath;

    //Create the needed local directory
    QDir parentDir( localParentDir );
    parentDir.mkpath( localParentDir );

    //Get the directory listing
    QSharedPointer<DirectoryListing> listing = m_DownloadThread->onGetDirectoryListingSlot( remotePath );
    if( listing.isNull() )
    {
        emit downloadAbortedSignal( "The path " + remotePath + " couldn't be found" );
        return false;
    }

    //Add all the files we have to the list
    if( listing->Entries().size() )
    {
        for( auto entry: listing->Entries() )
        {
            if( entry->isDirectory() )
            {
                m_RemoteDirectoriesToProcess.append( entry->Path() );
            }else
            {
                QString localFilePath = m_LocalDestination + "/" + relativeRemotePath + "/" + entry->Name();
                m_DownloadList[ localFilePath ] = entry;
            }
        }
    }else
    {
        return false;
    }

    return true;
}
