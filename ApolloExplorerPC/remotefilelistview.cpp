#include "remotefilelistview.h"
#include "qdragremote.h"
#include "mouseeventfilter.h"
#include "remotefilemimedata.h"

#include <QApplication>
#include <QDrag>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QHeaderView>
#include <QDir>
#include <QStyledItemDelegate>
#include <QImage>

#define DEBUG 0
#include "AEUtils.h"


class ItemStyleDelegate : public QStyledItemDelegate {
 public:
  ItemStyleDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QFontMetrics fm(option.font);
    const QAbstractItemModel* model = index.model();
    QString Text = model->data(index, Qt::DisplayRole).toString();
    QRect neededsize = fm.boundingRect( option.rect, Qt::TextWordWrap,Text );
    QRect iconSize = model->data(index, Qt::DecorationRole ).toRect();
    return QSize(option.rect.width(), neededsize.height());
  }
};

ItemStyleDelegate *g_ItemStyleDelegate = nullptr;

RemoteFileListView::RemoteFileListView( QWidget *parent ) :
    QListView( parent )
{
    this->setDragEnabled( true );
    this->setAcceptDrops( true );
    m_DropTimer.setSingleShot( false );

    //Signals and slots
    connect( this, &RemoteFileListView::doubleClicked, this, &RemoteFileListView::onItemDoubleClicked );

    //Create a dummy model so we can at least set the header width
    QSharedPointer<DirectoryListing> directoryListing( new DirectoryListing() );
    directoryListing->setName( "Waiting for server.");
    m_TemporaryModel = new RemoteFileTableModel( directoryListing );
    this->setModel( m_TemporaryModel );

    //Setup how the icons should be displayed$
    //g_ItemStyleDelegate = new ItemStyleDelegate( this );
    //setItemDelegate( g_ItemStyleDelegate );
    setViewMode( QListView::IconMode );
    setSpacing( 10 );
    setGridSize( QSize( 160, 100 ) );
    setWrapping( true );
    setWordWrap( true );
    setStyleSheet( parent->styleSheet() );


    //setSizeAdjustPolicy( QAbstractScrollArea::AdjustToContents );
    setSizeAdjustPolicy( AdjustToContents );
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    //setSelectionBehavior( QAbstractItemView::SelectRows );
}

RemoteFileListView::~RemoteFileListView()
{
    delete m_TemporaryModel;
}

void RemoteFileListView::dragLeaveEvent( QDragLeaveEvent *e )
{
    e->accept();
}

void RemoteFileListView::dragEnterEvent( QDragEnterEvent *e )
{
    //DBGLOG << "Drag enter event: " << e->type();
    e->accept();
}

void RemoteFileListView::dropEvent( QDropEvent *e )
{
    DBGLOG << "Action: " << e->type();
    //if( e->source() == this )   return;     //Don't drop to yourself.

    e->acceptProposedAction();

    //By default, we will upload to the root of the view
    const RemoteFileTableModel *model = dynamic_cast<RemoteFileTableModel*>( this->model() );
    QSharedPointer<DirectoryListing> destinationListing = model->getRootDirectoryListing();

    //Check what files lay beneath the mouse
    QModelIndex index = this->indexAt( e->pos() );
    if( index.isValid() )
    {
        //Check if this is a file or a directory
        if( model->getDirectoryListingForIndex( index )->Type() == DET_USERDIR )
        {
            //this->selectRow( index.row() );
            this->setCurrentIndex( index );
            destinationListing = model->getDirectoryListingForIndex( index );
        }
    }

    DBGLOG << "Uploading to " << destinationListing->Path();

    //Build a list of files to upload
    QStringList localFiles;
    foreach (const QUrl &url, e->mimeData()->urls())
    {
        //Local file path
        QString localFilePath = url.toLocalFile();
        DBGLOG << "Dropped file:" << localFilePath;
        localFiles << localFilePath;
    }

    //Start the upload
    if( !m_UploadDialog.isNull() )
    {
        m_UploadDialog->startUpload( destinationListing, localFiles );
    }
}

void RemoteFileListView::dragMoveEvent(QDragMoveEvent *e)
{
    DBGLOG << "Action: " << e->type();
    //e->setDropAction( Qt::MoveAction );
    if( e->source() == this )   return;     //Igore dragging to self
    e->accept();

    //By default, we will upload to the root of the view
    const RemoteFileTableModel *model = dynamic_cast<RemoteFileTableModel*>( this->model() );
    QSharedPointer<DirectoryListing> destinationListing = model->getRootDirectoryListing();

    //Check what files lay beneath the mouse
    QModelIndex index = this->indexAt( e->pos() );
    if( index.isValid() )
    {
        //Check if this is a file or a directory
        if( model->getDirectoryListingForIndex( index )->Type() == DET_USERDIR )
        {
            //this->selectRow( index.row() );
            this->setCurrentIndex( index );
            destinationListing = model->getDirectoryListingForIndex( index );
        }else
        {
            this->clearSelection();
        }
    }
}

void RemoteFileListView::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED( supportedActions )
    DBGLOG << "startDrag event.";

#if 1
    //Set up the mimedata
    RemoteFileMimeData* newMimeData = new RemoteFileMimeData();
    newMimeData->setDownloadDialog( m_DownloadDialog );
    newMimeData->setAction( Qt::MoveAction );

    //Get the list of selected items
    auto selectedItems = this->selectedIndexes();
    quint64 fileSize = 0;
    for( auto item = selectedItems.begin(); item != selectedItems.end(); item++ )
    {
        RemoteFileTableModel *model = dynamic_cast<RemoteFileTableModel*>( this->model() );
        QSharedPointer<DirectoryListing> listing = model->getDirectoryListingForIndex( *item );
        if( !listing.isNull() )
        {
            DBGLOG << "Item at row " << item->row() << ": " << listing->Name();
            newMimeData->addRemotePath( listing );
            fileSize+=listing->Size();
        }
        else
            DBGLOG << "Item at row " << item->row() << ": NONAME";
    }

    //Now check the file size and see if we need to block the DND or switch to a dialog
    m_Settings->beginGroup( SETTINGS_GENERAL );
    quint64 DNDSizeLimit = m_Settings->value( SETTINGS_DND_SIZE, 20 ).toInt() * 1024 *1024;
    QString DNDOperation = m_Settings->value( SETTINGS_DND_OPERATION, SETTINGS_DND_OPERATION_DOWNLOAD_DIALOG ).toString();
    m_Settings->endGroup();

    //Can we continue with DND?
    if(     DNDOperation == SETTINGS_DND_OPERATION_CONTINUE ||
            fileSize <= DNDSizeLimit )
    {
        //Create the new drag action
        QDragRemote *drag = new QDragRemote( this );
        drag->setMimeData( newMimeData );
        drag->exec( Qt::MoveAction, Qt::MoveAction );
        m_QDragList.append( drag );
        m_CurrentDrag = drag;
        connect( drag, &QDrag::destroyed, [&]( QObject *object ){ Q_UNUSED( object); m_CurrentDrag = nullptr; } );

    }else if( DNDOperation == SETTINGS_DND_OPERATION_DOWNLOAD_DIALOG )
    {
        connect( newMimeData, &RemoteFileMimeData::dropHappenedSignal, this, &RemoteFileListView::downloadViaDownloadDialogSignal );
        newMimeData->setSendURLS( false );

        //Create the new drag action
        QDragRemote *drag = new QDragRemote( this );
        drag->setMimeData( newMimeData );
        drag->exec( Qt::MoveAction, Qt::MoveAction );
        m_QDragList.append( drag );
        m_CurrentDrag = drag;
        connect( drag, &QDrag::destroyed, [&]( QObject *object ){ Q_UNUSED( object); m_CurrentDrag = nullptr; } );

        //emit downloadViaDownloadDialogSignal();
    }
#endif
}

void RemoteFileListView::mouseMoveEvent(QMouseEvent *e)
{
    DBGLOG << "MouseMoveEvent: " << e->type();
    if( ! (e->buttons() | Qt::LeftButton ) )
    {
        DBGLOG << "Mouse event: Left mouse button did something.";
    }
    if( !MouseEventFilterSingleton::getInstance()->isLeftMouseButtonDown() )
    {
        DBGLOG << "Mouse drop event outside of the application";
    }
    QListView::mouseMoveEvent( e );
}

void RemoteFileListView::mouseReleaseEvent(QMouseEvent *e)
{
    DBGLOG << "MouseReleaseEvent: " << e->type();
    if( ! (e->buttons() | Qt::LeftButton ) )
    {
        DBGLOG << "Mouse event: Left mouse button did something.";
    }
    QListView::mouseReleaseEvent( e );
}

QList<QSharedPointer<DirectoryListing> > RemoteFileListView::getSelectedItems()
{
    //Get the list of selected items
    QList<QSharedPointer<DirectoryListing>> selectedDirectoryListings;
    auto selectedItems = this->selectedIndexes();
    for( auto item = selectedItems.begin(); item != selectedItems.end(); item++ )
    {
        RemoteFileTableModel *model = dynamic_cast<RemoteFileTableModel*>( this->model() );
        QSharedPointer<DirectoryListing> listing = model->getDirectoryListingForIndex( *item );

        if( !listing.isNull() )
        {
            //We will get a listing for every column in the table.
            //So filter out each duplicate.
            if( selectedDirectoryListings.contains( listing ) ) continue;

            selectedDirectoryListings << listing;
            DBGLOG << "Item at row " << item->row() << ": " << listing->Name();
        }
        else
            DBGLOG << "Item at row " << item->row() << ": NONAME";
    }

    //Now let the caller know what was selected
    return selectedDirectoryListings;
}

void RemoteFileListView::setDownloadDialog(QSharedPointer<DialogDownloadFile> dialog)
{
    m_DownloadDialog = dialog;
}

void RemoteFileListView::setUploadDialog(QSharedPointer<DialogUploadFile> dialog)
{
    m_UploadDialog = dialog;
}

void RemoteFileListView::setSettings(QSharedPointer<QSettings> settings)
{
    m_Settings = settings;
    dynamic_cast<RemoteFileTableModel*>( this->model() )->setSettings( settings );
}

void RemoteFileListView::onItemDoubleClicked(const QModelIndex &index)
{
    Q_UNUSED( index)
    //Get the list of selected items
    QList<QSharedPointer<DirectoryListing>> selectedDirectoryListings;
    auto selectedItems = this->selectedIndexes();
    for( auto item = selectedItems.begin(); item != selectedItems.end(); item++ )
    {
        RemoteFileTableModel *model = dynamic_cast<RemoteFileTableModel*>( this->model() );
        QSharedPointer<DirectoryListing> listing = model->getDirectoryListingForIndex( *item );

        if( !listing.isNull() )
        {
            //We will get a listing for every column in the table.
            //So filter out each duplicate.
            if( selectedDirectoryListings.contains( listing ) ) continue;

            selectedDirectoryListings << listing;
            DBGLOG << "Item at row " << item->row() << ": " << listing->Name();
        }
        else
        {
            DBGLOG << "Item at row " << item->row() << ": NONAME";
        }
    }

    //Now let the parent know what was selected
    emit itemsDoubleClicked( selectedDirectoryListings );
}

void RemoteFileListView::dropTimerTimeoutSlot()
{
    //Check if the mouse has been released
    if( !MouseEventFilterSingleton::getInstance()->isLeftMouseButtonDown() )
    {
        DBGLOG << "Drop has occurred";
        m_DropTimer.stop();

        //Get the temporary path
        QString tempPath( QDir::tempPath().toLatin1() );
        tempPath.append( "/ApolloExporer" );
        QDir tempDir( tempPath );
        if( !tempDir.exists() )
        {
            tempDir.mkpath( tempPath );
        }

        //Get the list of selected items
        QList<QSharedPointer<DirectoryListing> > remotePaths;
        auto selectedItems = this->selectedIndexes();
        for( auto item = selectedItems.begin(); item != selectedItems.end(); item++ )
        {
            RemoteFileTableModel *model = dynamic_cast<RemoteFileTableModel*>( this->model() );
            QSharedPointer<DirectoryListing> listing = model->getDirectoryListingForIndex( *item );
            if( !listing.isNull() )
            {
                DBGLOG << "Item at row " << item->row() << ": " << listing->Name();
                remotePaths.append( listing );
            }
            else
                DBGLOG << "Item at row " << item->row() << ": NONAME";
        }

        DBGLOG << "Starting download";
        QEventLoop loop;
        connect( m_DownloadDialog.get(), &DialogDownloadFile::allFilesDownloaded, &loop, &QEventLoop::quit );
        m_DownloadDialog->startDownload( remotePaths, tempPath );
        this->setEnabled( false );
        loop.exec();
        this->setEnabled( true );
        DBGLOG << "Hopefully the download finished";


        //Create a new mimedata
        QMimeData *mimeData = new QMimeData();

        //Now we need to build a list of path pairs with a local destination for a remote path
        QList<QUrl> urlList;
        QListIterator<QSharedPointer<DirectoryListing>> iter( remotePaths );
        while( iter.hasNext() )
        {
            auto entry = iter.next();
            QString localPath = tempPath + "/" + entry->Name();
            QString remotePath = entry->Path();
            DBGLOG << "Will copy " << remotePath << " to " << localPath;

            //If the requested data is a directory, we should create a temporary directory here
            if( entry->Type() == DET_USERDIR )
            {
                QDir dir;
                dir.mkdir( localPath );
            }

            //Now build the local file URL
            QUrl url = QUrl::fromLocalFile( localPath );
            if( !urlList.contains( url ) )
            {
                urlList.append( url );
            }else
            {
                WARNLOG << "Duplicate entry " << entry->Path();
            }
        }
        mimeData->setUrls( urlList );


        //Create the new drag action
        QDragRemote *drag = new QDragRemote( this );
        drag->setMimeData( mimeData );
        drag->exec( Qt::MoveAction );
        QMouseEvent *mEvnRelease = new QMouseEvent(QEvent::MouseButtonRelease, QCursor::pos(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent( this->parent() ,mEvnRelease);
        DBGLOG <<"Currentl drag queue is: " << m_QDragList.size();
    }
}
