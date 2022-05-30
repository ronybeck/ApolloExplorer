#include "remotefilemimedata.h"

#define DEBUG 1
#include "AEUtils.h"
#include "mouseeventfilter.h"

#include <QApplication>

//Linux specific stuff
#if __linux__
extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <linux/input.h>
#include <fcntl.h>
#include <X11/Xlib.h>
}
#endif


#if __linux__
bool PeekerCallback( xcb_generic_event_t *event, void *peekerData )
{
    DBGLOG << "Got an XCB event.";
    switch( event->response_type )
    {
        case XCB_BUTTON_PRESS:
        {
            xcb_button_press_event_t *bp = (xcb_button_press_event_t *)event;
            switch( bp->detail )
            {
                case 1:
                {
                    DBGLOG << "Left mouse button pressed.";
                    RemoteFileMimeData *mimeData = reinterpret_cast<RemoteFileMimeData*>( peekerData );
                    mimeData->m_LeftMouseButtonDown = true;
                    break;
                }
                default:
                break;
            }
        }
        case XCB_BUTTON_RELEASE:
        {
            DBGLOG << "Left mouse button released.";
            xcb_button_press_event_t *bp = (xcb_button_press_event_t *)event;
            switch( bp->detail )
            {
                case 1:
                {
                    RemoteFileMimeData *mimeData = reinterpret_cast<RemoteFileMimeData*>( peekerData );
                    mimeData->m_LeftMouseButtonDown = true;
                    break;
                }
                default:
                break;
            }
        }
        default:
        break;
    }

    return false;
}
#endif

RemoteFileMimeData::RemoteFileMimeData()
{
    m_Action = Qt::IgnoreAction;
    m_LeftMouseButtonDown = true;
    m_TempFilePath = QDir::tempPath() + "/ApolloExplorer/";

#if __linux__
//    Window root, child;
//    int rootX, rootY, winX, winY;
//    unsigned int mask;

//    #define MOUSEFILE "/dev/input/mice"


    //dpy = XOpenDisplay(NULL);
    //Display *xDisplay = QX11Info::display();
//    qint32 peakerID = QX11Info::generatePeekerId();
//    if( !QX11Info::peekEventQueue( PeekerCallback, this, QX11Info::PeekFromCachedIndex, peakerID ) )
//    {
//        DBGLOG << "XCB Failed to add peaker callback";
//    }
    //XQueryPointer(xDisplay,DefaultRootWindow(xDisplay),&root,&child, &rootX,&rootY,&winX,&winY,&mask);

//    if((fd = open(MOUSEFILE, O_RDONLY | O_NONBLOCK )) == -1)
//    {
//      perror("opening device");
//      exit(EXIT_FAILURE);
//    }

    //Set non-blocking IO
    //int flags = fcntl( fd, F_GETFL);
    //fcntl( fd, F_SETFL, flags | O_NONBLOCK );
#endif
}

RemoteFileMimeData::~RemoteFileMimeData()
{
#if __linux__
    //close( fd );
#endif
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

    //Is the mouse button released now?
    if( MouseEventFilterSingleton::getInstance()->isLeftMouseButtonDown() )
    {
        DBGLOG << "Sorry, but the mouse button is still down";
        return QVariant();
    }

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
        m_DownloadList.append( QPair<QString,QString>( localPath, remotePath ) );

        //If the requested data is a directory, we should create a temporary directory here
        if( entry->Type() == DET_USERDIR )
        {
            QDir dir;
            dir.mkdir( localPath );
        }

        //Now build the local file URL
        QUrl url = QUrl::fromLocalFile( localPath );
        if( !m_LocalUrls.contains( url ) ) m_LocalUrls.push_back( url );
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
#if __linux__
    /*
    struct input_event ie;
    while(read(fd, &ie, sizeof(struct input_event)))
    {
        if (ie.type == 1)
        {
          if (ie.code == 272 )
          {
            printf("Mouse button ");
            if (ie.value == 0)
            {
              printf("released!!\n");
            }
            if (ie.value == 1)
            {
              printf("pressed!!\n");
            }
        } else {
            printf("time %ld.%06ld\ttype %d\tcode %d\tvalue %d\n",
                ie.time.tv_sec, ie.time.tv_usec, ie.type, ie.code, ie.value);
        }
      }
    }
*/
#endif
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

