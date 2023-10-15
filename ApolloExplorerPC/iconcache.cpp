#include "iconcache.h"

#define LOCK QMutexLocker locker( &m_Mutex );
#define UNLOCK locker.unlock();
#define RELOCK locker.relock();

#define RAM_DISK_ICON_PATH_WITH_INFO "ENVARC:sys/def_ram.info"
#define RAM_DISK_ICON_PATH "ENVARC:sys/def_ram"

IconCache::IconCache(QObject *parent)
    : QObject{parent},
    m_Mutex( QMutex::Recursive ),
    m_Cache( ),
    m_Connected( false ),
    m_DownloadInProgress( false ),
    m_IconThread( new IconThread( this ) )
{
    //Connect signals and slots
    connect( this, &IconCache::connectToHostSignal, m_IconThread, &IconThread::onConnectToHostSlot );
    connect( this, &IconCache::disconnectedFromHostSignal, m_IconThread, &IconThread::onDisconnectedFromHostSlot );
    connect( this, &IconCache::getIconSignal, m_IconThread, &IconThread::onGetIconSlot );
    connect( m_IconThread, &IconThread::connectedToServerSignal, this, &IconCache::onConnectedToHostSlot );
    connect( m_IconThread, &IconThread::disconnectedFromServerSignal, this, &IconCache::onDisconnectedFromHostSlot );
    connect( m_IconThread, &IconThread::iconFileSignal, this, &IconCache::onIconReceivedSlot );
    connect( m_IconThread, &IconThread::operationTimedOutSignal, this, &IconCache::onOperationTimedOut );
    connect( m_IconThread, &IconThread::abortedSignal, this, &IconCache::onAbortSlot );

    //Run the icon thread
    m_IconThread->start();

}

void IconCache::clearCache()
{
    LOCK;
    m_Cache.clear();
}

IconCache::~IconCache()
{
    m_IconThread->stopThread();
    m_IconThread->wait();
    delete m_IconThread;
}

QSharedPointer<AmigaInfoFile> IconCache::getIcon( QString filepath )
{
    //First normalise this and remove the ".info"
    if( filepath.endsWith( ".info" ) )
    {
        filepath = filepath.chopped( 5 );
    }

    //Check the cache first
    for( QMap<QString, QSharedPointer<AmigaInfoFile>>::Iterator iter = m_Cache.begin(); iter != m_Cache.end(); iter++ )
    {
        if( iter.key() == filepath )
        {
            //We found a cached icon.  Return that
            return iter.value();
        }
    }

    //Nothing to return;
    return nullptr;
}

void IconCache::onConnectToHostSlot(QHostAddress host, quint16 port)
{
    emit connectToHostSignal( host, port );
}

void IconCache::onDisconnectFromHostSlot()
{
    emit disconnectFromHostSignal();
}

void IconCache::retrieveIconSlot(QString filepath)
{
    LOCK;
    bool isInfoFile = false;

    //Hack!  When getting the ram disk, look for the file elsewhere.  It is stored in ENVARC:
    if( !filepath.compare( "Ram Disk:Disk.info" ) || !filepath.compare( "Ram Disk:Disk" ) )
    {
        filepath=RAM_DISK_ICON_PATH_WITH_INFO;
    }

    //First normalise this and remove the ".info"
    if( filepath.endsWith( ".info" ) )
    {
        filepath = filepath.chopped( 5 );
        isInfoFile = true;
    }

    //Check the cache first
    for( QMap<QString, QSharedPointer<AmigaInfoFile>>::Iterator iter = m_Cache.begin(); iter != m_Cache.end(); iter++ )
    {
        if( iter.key() == filepath )
        {
            //We found a cached icon.  Send that
            emit iconSignal( iter.key(), iter.value() );
            return;
        }
    }

    //Should we really append ".info" to the file path and try and download it?  Maybe not.
    if( !isInfoFile )   return;

    //Otherwise we need to add this to the list of things to download.  But we should re-add the ".info"
    filepath.append( ".info" );
    m_DownloadList.push_back( filepath );

    //Now if we are not already downloading something, we should kick that off now
    if( !m_DownloadInProgress )
    {
        //Get the first file from the list
        QString nextIconPath = m_DownloadList.first();
        m_DownloadList.pop_front();

        //Emit the signal to trigger the download
        m_DownloadInProgress = true;
        emit getIconSignal( nextIconPath );
    }
}

void IconCache::onConnectedToHostSlot()
{
    m_Connected = true;
}

void IconCache::onDisconnectedFromHostSlot()
{
    m_Connected = false;
    m_DownloadInProgress = false;
}

void IconCache::onIconReceivedSlot( QString filepath, QSharedPointer<AmigaInfoFile> icon )
{
    LOCK;

    //First we should add this to our cache
    //But we want to store it without the .info in the file path
    //We can always search for both the file and file.info
    if( filepath.endsWith( ".info" ) )
    {
        filepath = filepath.chopped( 5 );
    }

    //Hack!  We need to translate the ram disk icon path back to "Ram Disk:Disk.info"
    if( !filepath.compare( RAM_DISK_ICON_PATH_WITH_INFO ) || !filepath.compare( RAM_DISK_ICON_PATH ) )
    {
        //Reset this back to the ram disk path
        filepath = "Ram Disk:Disk";
    }

    //Store this in our cace
    m_Cache[ filepath ] = icon;

    qDebug() << "Got icon for " << filepath;


    //Now emit a signal with this icon
    emit iconSignal( filepath, icon );

    //Have we finished downloading all icons?
    if( m_DownloadList.empty() )
    {
        //We are done
        m_DownloadInProgress = false;
        return;
    }

    //It seems we have more icons to get
    QString nextPath = m_DownloadList.front();
    m_DownloadList.pop_front();
    emit getIconSignal( nextPath );
}

void IconCache::onOperationTimedOut()
{
    m_DownloadInProgress = false;
}

void IconCache::onAbortSlot( QString reason )
{
    Q_UNUSED( reason )
    //Ok something went wrong.  Let's just move on to the next icon
    if( m_DownloadList.empty() )
    {
        //We are done
        m_DownloadInProgress = false;
        return;
    }

    //It seems we have more icons to get
    QString nextPath = m_DownloadList.front();
    m_DownloadList.pop_front();
    emit getIconSignal( nextPath );
}
