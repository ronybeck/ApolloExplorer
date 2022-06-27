#include "remotefilemimedata.h"
#ifdef __MINGW32__
#include "windows.h"
#endif
#define DEBUG 1
#include "AEUtils.h"
#include "mouseeventfilter.h"

#include <QApplication>


#define LOCK QMutexLocker locker( &m_Mutex )
#define UNLOCK locker.unlock()
#define RELOCK locker.relock();


RemoteFileMimeData::RemoteFileMimeData() :
    m_Mutex( QMutex::Recursive ),
    m_Action( Qt::IgnoreAction ),
    m_TempFilePath( QDir::tempPath() + "/ApolloExplorer/" ),
    m_LocalUrls( ),
    m_DownloadList( ),
    m_DownloadDialog( nullptr ),
    m_RemotePaths( ),
    m_LeftMouseButtonDown( true ),
    m_DataRetreived( false )
{

}

RemoteFileMimeData::~RemoteFileMimeData()
{
    DBGLOG << "Destroying";
}

QVariant RemoteFileMimeData::retrieveData(const QString &mimeType, QVariant::Type type) const
{
    DBGLOG << "Requested QVariant mime type : " << mimeType << " of QVariant type: " << type;

    //First, check that we are ready to do the drop
    if( m_Action != Qt::MoveAction )
    {
        //Not ready yet.
        DBGLOG << "Action is currently: " << m_Action;
        return QVariant();
    }

    //Here we need to start doing the file download to a temp directory
    if( m_DownloadDialog.isNull() )
    {
        qDebug() << "No download dialog.  We can't download the files.";
        return QVariant();  //No means to download the files
    }

    //Make sure we have some local URLs
    if( m_LocalUrls.empty() )
    {
        return QVariant();
    }

    //Is a download already in progress?
    if( m_DownloadDialog->isCurrentlyDownloading() )
    {
        qDebug() << "A download is in progress already.";
        return QVariant();
    }

    //We have set some URLs?
    if( !hasUrls() )
    {
        qDebug() << "There are no file URLs set!";
        return QVariant();
    }

#ifdef __MINGW32__
    if(((unsigned short)GetAsyncKeyState(VK_LBUTTON))>1)
    {
        return QMimeData::retrieveData( mimeType, type );
    }
#else
    //Is the mouse button released now?
    if( MouseEventFilterSingleton::getInstance()->isLeftMouseButtonDown() )
    {
        DBGLOG << "Sorry, but the mouse button is still down";
        return QVariant();
    }
#endif

    //if we already downloaded this, then we don't need to do anything more
    if( m_DataRetreived )
        return QMimeData::retrieveData( mimeType, type );

    m_DataRetreived = true;

    //Create the temporary directory
    QDir tmpDir( m_TempFilePath );
    tmpDir.mkdir( m_TempFilePath );

    //Start the download
    DBGLOG << "Starting download";
    QEventLoop loop;
    connect( m_DownloadDialog.get(), &DialogDownloadFile::allFilesDownloaded, &loop, &QEventLoop::quit );
    m_DownloadDialog->startDownload( m_RemotePaths, m_TempFilePath );
    loop.exec();
    DBGLOG << "Hopefully the download finished";

    return QMimeData::retrieveData( mimeType, type );
}

bool RemoteFileMimeData::hasFormat(const QString &mimeType) const
{
    DBGLOG << "Asked for mime type " << mimeType;
    //We only need to support the URI for windows and linux (and mac?)
    if( mimeType == "text/uri-list" )
    {
        if( m_TempFilePath.length() > 0 )
        {
            DBGLOG << "We have a text/uri-list to give";
            return true;
        }
        DBGLOG << "We don't have a text/uri-list to give at this time.";
    }
    return false;
}

void RemoteFileMimeData::setAction(Qt::DropAction action)
{
    DBGLOG << "Setting action to: " << action;
    m_Action = action;
}

void RemoteFileMimeData::setTempFilePath(const QString &path)
{
    DBGLOG << "Setting the temp file path to: " << path;
    m_TempFilePath = path;
}

void RemoteFileMimeData::addRemotePath( const QSharedPointer<DirectoryListing> remotePath )
{
    DBGLOG << "Adding remote path " << remotePath->Path();
    if( m_RemotePaths.contains( remotePath ) )
    {
        DBGLOG << "Remote path is already there!";
        return;
    }

    //Add this remote path
    m_RemotePaths.push_back( remotePath );

    //Now we need to build a list of path pairs with a local destination for a remote path
    QListIterator<QSharedPointer<DirectoryListing>> iter( m_RemotePaths );
    while( iter.hasNext() )
    {
        auto entry = iter.next();
        QString localPath = m_TempFilePath + "/" + entry->Name();
        QString remotePath = entry->Path();
        DBGLOG << "Will copy " << remotePath << " to " << localPath;

        //Make sure we don't already have this in our list
        if( m_DownloadList.contains(QPair<QString,QString>( localPath, remotePath )))   continue;
        m_DownloadList.append( QPair<QString,QString>( localPath, remotePath ) );

        //If the requested data is a directory, we should create a temporary directory here
        if( entry->Type() == DET_USERDIR )
        {
            QDir dir;
            dir.mkdir( localPath );
        }

        //Now build the local file URL
        QUrl url = QUrl::fromLocalFile( localPath );
        if( !m_LocalUrls.contains( url ) &&
                !this->urls().contains( url ) )
        {
            m_LocalUrls.push_back( url );
        }
    }

    if( m_LocalUrls.size() )
    {
        this->setUrls( m_LocalUrls );
    }
}

void RemoteFileMimeData::setDownloadDialog(QSharedPointer<DialogDownloadFile> dialog)
{
    m_DownloadDialog = dialog;
}

bool RemoteFileMimeData::isLeftMouseButtonDown() const
{
    return m_LeftMouseButtonDown;
}

void RemoteFileMimeData::onLeftMouseButtonPressedSlot()
{
    m_LeftMouseButtonDown = true;
}

void RemoteFileMimeData::onLeftMouseButtonReleasedSlot()
{
    m_LeftMouseButtonDown = false;
}

