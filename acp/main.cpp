#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QSettings>
#include <QThread>
#include <iostream>
#include <memory>

#define DEBUG 0
#include "AEUtils.h"
#include "protocolTypes.h"
#include "hostlister.h"
#include "directorylister.h"
#include "fileuploader.h"
#include "filedownloader.h"


QStringList getHostList( QSharedPointer<QSettings> settings, const QStringList positionArguments )
{
    QStringList hosts;

    //Create a host lister object
    HostLister hostLister( settings, nullptr );
    DBGLOG << "Listing hosts";

    //Wait for the hostlist signal to come
    bool scanCompleted = false;
    QVector<QSharedPointer<AmigaHost>> hostList;
    QObject::connect( &hostLister, &HostLister::hostsDiscoveredSignal, [&]( QVector<QSharedPointer<AmigaHost>> hostsFound ) {
        scanCompleted = true;
        hostList = hostsFound;
    });

    //Perform the wait
    while( !scanCompleted )
    {
        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    //If the user supplied a search pattern, check for that
    //We will assume, in this case, that the first argument is the search pattern
    QString searchPattern;
    if( positionArguments.size() > 0 )
        searchPattern = positionArguments[ 0 ];
    if( searchPattern.length() > 0 )
    {
        QStringList matches = hostLister.getHostLike( searchPattern );

        //Now print the list
        foreach(  auto match, matches )
        {
            hosts.append( match );
        }

    }else
    {
        //Just print the full list
        QVectorIterator<QSharedPointer<AmigaHost>> iter( hostList );
        while( iter.hasNext()  )
        {
            QSharedPointer<AmigaHost> host = iter.next();
            hosts.append( host->Name() + " " + host->Address().toString() );
        }
    }

    return hosts;
}

bool performDownload( QSharedPointer<QSettings> settings, QStringList arguments, bool recursive )
{
    //How many arguments do we have?
    int numberOfArguments = arguments.size();
    QString sourcePath = arguments[ 0 ];
    QString destinationPath = arguments[ 1 ];

    //We need to get the host and remote path out of the last argument
    QStringList elements = sourcePath.split( ":" );
    QString host = elements[ 0 ];
    QString remotePath = elements[ 1 ];

    //If the host is already an IP address, no lookup is needed
    QRegExp ipAddressRegExp( "^(?:[0-9]{1,3}.){3}[0-9]{1,3}$" );
    QString ipAddressString = host;
    if( !ipAddressRegExp.exactMatch( host ) )
    {
        //Now check for a host with this hostname
        QStringList hosts = getHostList( settings, QStringList { host } );
        if( hosts.size() != 1 )
        {
            std::cerr << "Either the hostname " << host.toStdString() << " is ambiguous or could not be found." << std::endl;
            return false;
        }

        //We need the ip address
        QStringList elements = hosts[ 0 ].split( " " );
        if( elements.size() != 2 )
        {
            std::cerr << "We couldn't find an IP address for host " << host.toStdString() << "." << std::endl;
            return false;
        }
        ipAddressString = elements[ 1 ];
    }

    //Create the file uploader object
    std::unique_ptr<FileDownloader> fileDownloader( new FileDownloader( remotePath, destinationPath, ipAddressString ) );
    fileDownloader->setRecursive( recursive );


    //Connect to the host
    if( !fileDownloader->connectToHost() )
    {
        std::cerr << "Failed to connect to host" << std::endl;
        return false;
    }
    DBGLOG << "Connected!";

    //Wait for the hostlist signal to come
    bool uploadEnded = false;
    bool error = false;
    QObject::connect( fileDownloader.get(), &FileDownloader::downloadAbortedSignal, [&]( QString reason ) {
        uploadEnded = true;
        error = true;
        std::cerr << "Error: " << reason.toStdString() << std::endl;
    });
    QObject::connect( fileDownloader.get(), &FileDownloader::downloadCompletedSignal, [&]() {
        uploadEnded = true;
    });

    //Perform the wait
    fileDownloader->startDownload();
    while( !uploadEnded )
    {
        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    //Return
    if( error ) return false;
    return true;
}

bool performUpload( QSharedPointer<QSettings> settings, QStringList arguments, bool recursive )
{
    //How many arguments do we have?
    int numberOfArguments = arguments.size();
    QString argN = arguments[ numberOfArguments -1 ];

    //We need to get the host and remote path out of the last argument
    QStringList elements = argN.split( ":" );
    QString host = elements[ 0 ];
    QString remotePath = elements[ 1 ];

    //Compost a list of source paths
    QStringList sourcePaths;
    for( const auto& sourcePath: arguments )
    {
        sourcePaths.append( sourcePath );
    }
    sourcePaths.pop_back(); //Drop the last one in the list as this will be the destination;

    //If the host is already an IP address, no lookup is needed
    QRegExp ipAddressRegExp( "^(?:[0-9]{1,3}.){3}[0-9]{1,3}$" );
    QString ipAddressString = host;
    if( !ipAddressRegExp.exactMatch( host ) )
    {
        //Now check for a host with this hostname
        QStringList hosts = getHostList( settings, QStringList { host } );
        if( hosts.size() != 1 )
        {
            std::cerr << "Either the hostname " << host.toStdString() << " is ambiguous or could not be found." << std::endl;
            return false;
        }

        //We need the ip address
        QStringList elements = hosts[ 0 ].split( " " );
        if( elements.size() != 2 )
        {
            std::cerr << "We couldn't find an IP address for host " << host.toStdString() << "." << std::endl;
            return false;
        }
        ipAddressString = elements[ 1 ];
    }

    //Create the file uploader object
    std::unique_ptr<FileUploader> fileUploader( new FileUploader(sourcePaths, remotePath, ipAddressString ) );
    fileUploader->setRecursive( recursive );


    //Connect to the host
    if( !fileUploader->connectToHost() )
    {
        std::cerr << "Failed to connect to host" << std::endl;
        return false;
    }
    DBGLOG << "Connected!";

    //Wait for the hostlist signal to come
    bool uploadEnded = false;
    bool error = false;
    QObject::connect( fileUploader.get(), &FileUploader::uploadAbortedSignal, [&]( QString reason ) {
        uploadEnded = true;
        error = true;
        std::cerr << "Error: " << reason.toStdString() << std::endl;
    });
    QObject::connect( fileUploader.get(), &FileUploader::uploadCompletedSignal, [&]() {
        uploadEnded = true;
    });

    //Perform the wait
    fileUploader->startUpload();
    while( !uploadEnded )
    {
        QThread::msleep( 10 );
        QCoreApplication::processEvents();
    }

    //Return
    if( error ) return false;
    return true;
}

QStringList getDirectoryListing( QSharedPointer<QSettings> settings, const QStringList positionArguments )
{
    QStringList contents;
    QString host = "";
    QString path = "";

    //First we need to extract the hostname out of the first parameter
    QString arg1 = positionArguments[ 0 ];

    //Case 1 - with semi-colon.  The path should be specified as host:path
    if( arg1.contains( ":" ) && !arg1.endsWith( ":" ) )
    {
        QStringList elements = arg1.split( ":" );
        host = elements[ 0 ];
        path = elements [ 1 ];

        //Since we are using the ":" character to delimit the host and path
        //we need to replace the first "/" in the path with a ":" because that is the drive!
        if( !path.contains( "/" ) )
        {
            path += ":";
        }else
        {
            path = path.replace( path.indexOf( '/' ), 1, ":" );
        }
        DBGLOG << "Searching path " << path;
    }
    else    //All other cases we assume an empty path
    {
        //We just have a host
        host = arg1;

        //Strip any trailing ":"
        if( host.endsWith( ":" ) )
            host.chop( 1 );
    }

    //If the host is already an IP address, no lookup is needed
    QRegExp ipAddressRegExp( "^(?:[0-9]{1,3}.){3}[0-9]{1,3}$" );
    QString ipAddressString = host;
    if( !ipAddressRegExp.exactMatch( host ) )
    {
        //Now check for a host with this hostname
        QStringList hosts = getHostList( settings, QStringList { host } );
        if( hosts.size() != 1 )
        {
            std::cerr << "Either the hostname " << host.toStdString() << " is ambiguous or could not be found." << std::endl;
            return contents;
        }

        //We need the ip address
        QStringList elements = hosts[ 0 ].split( " " );
        if( elements.size() != 2 )
        {
            std::cerr << "We couldn't find an IP address for host " << host.toStdString() << "." << std::endl;
            return contents;
        }
        ipAddressString = elements[ 1 ];
    }

    //Now get a directory lister object
    DirectoryLister directoryLister( ipAddressString, path );
    contents = directoryLister.getDirectoryList();

    return contents;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName( "acp" );
    QCoreApplication::setApplicationVersion( VERSION_STRING );

    //Register some types for signalling
    qRegisterMetaType<QHostAddress>();
    qRegisterMetaType<QSharedPointer<DirectoryListing>>();

    //Get our settings.  Let's use the apollo explorer settings
    QSettings *_settings =  new QSettings( "ApolloTeam", "ApolloExplorer" );
    QSharedPointer<QSettings> settings( _settings );


    QCommandLineParser cmdParser;
    cmdParser.setApplicationDescription( "acp file transfer utility" );
    cmdParser.addHelpOption();
    cmdParser.addVersionOption();
    const QCommandLineOption listHostOption( {"l","list-hosts"},"List all hosts" );
    const QCommandLineOption listDirOption( {"d","list-directory"},"List all contents in a given path", "" );
    const QCommandLineOption recursiveOption( {"r","recursive"},"Copy recursively", "" );
    cmdParser.addOption( listHostOption );
    cmdParser.addOption( listDirOption );
    cmdParser.addOption( recursiveOption );
    cmdParser.addPositionalArgument( "source", "From where to copy" );
    cmdParser.addPositionalArgument( "destination", "To where to copy" );

    //parse the options
    cmdParser.process( app );

    //Did we get enough arguments
    if( argc < 2 )
    {
        cmdParser.showHelp( );
        return 0;
    }

    //Did the user want a list of hosts?
    if( cmdParser.isSet( listHostOption ) )
    {
        QStringList hosts = getHostList( settings, cmdParser.positionalArguments() );

        QStringListIterator iter( hosts );
        while( iter.hasNext() )
        {
            //Print the list
            QString host = iter.next();
            std::cout << host.toStdString() << std::endl;
        }

        return 0;
    }

    //Did the user want a directory lsiting
    if( cmdParser.isSet( listDirOption ) )
    {
        //Make sure we actually have some arguments to process
        if( cmdParser.positionalArguments().size() == 0 )
        {
            //Now arguments means no host
            std::cout << "Need a hostname to query" << std::endl;
            return -1;
        }

        //Ok, get the directory listing
        QStringList contents = getDirectoryListing( settings, cmdParser.positionalArguments() );

        //Print the directory listing
        QStringListIterator iter( contents );
        while( iter.hasNext() )
        {
            QString entry = iter.next();
            std::cout << entry.toStdString() << std::endl;
        }

        return 0;
    }

    //Down to business.  Do they want to do an upload or download?
    if( cmdParser.positionalArguments().size() >= 2 )
    {
        //How manay parameters were supplied?
        int numberOfArguments = cmdParser.positionalArguments().size();

        //So this is an upload or a download.   But which one?
        QString arg1 = cmdParser.positionalArguments()[ 0 ];
        QString argN = cmdParser.positionalArguments()[ numberOfArguments -1 ];


        //Uploads will have a ":" in the path
        if( argN.contains( ":" ) )
        {
            performUpload( settings, cmdParser.positionalArguments(), cmdParser.isSet( recursiveOption ) );
        }

        //Downloads will have a ":" in the first argument
        if( arg1.contains( ":" ) )
        {
            performDownload( settings, cmdParser.positionalArguments(), cmdParser.isSet( recursiveOption ) );
        }
    }


   // return app.exec();
}
