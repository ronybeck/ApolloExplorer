#include "remotefiletableview.h"
#include "remotefiletablemodel.h"
#include "qdragremote.h"
#include "mouseeventfilter.h"

#include <QApplication>
#include <QDrag>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QHeaderView>
#include <QDir>

#define DEBUG 0
#include "AEUtils.h"

RemoteFileTableView::RemoteFileTableView( QWidget *parent ) :
    QTableView( parent )
{
    //QTableView( parent );

    this->setDragEnabled( true );
    //this->setDragDropMode( DragDrop );
    this->setAcceptDrops( true );
    m_DropTimer.setSingleShot( false );

    //Signals and slots
    connect( this, &RemoteFileTableView::doubleClicked, this, &RemoteFileTableView::onItemDoubleClicked );
    //connect( &m_DropTimer, &QTimer::timeout, this, &RemoteFileTableView::dropTimerTimeoutSlot );

    //Create a dummy model so we can at least set the header width
    QSharedPointer<DirectoryListing> directoryListing( new DirectoryListing() );
    directoryListing->setName( "Waiting for server.");
    RemoteFileTableModel *temporaryModel = new RemoteFileTableModel( directoryListing );
    this->setModel( temporaryModel );

    verticalHeader()->setSectionResizeMode( QHeaderView::ResizeMode::ResizeToContents );
    verticalHeader()->setSizeAdjustPolicy( AdjustToContents );
    //For some reason, the stylesheet isn't propogated here in the DEB package.
    DBGLOG << "Header Style: " << horizontalHeader()->styleSheet();
//    this->setStyleSheet( "QHeaderView::section { background-color: red;}");
//    this->horizontalHeader()->setStyleSheet( "background-color: red;" );
    DBGLOG << "Header Style: " << this->styleSheet();
    //horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    this->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeMode::Stretch);
    this->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeMode::ResizeToContents );
    this->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeMode::ResizeToContents );
    this->verticalHeader()->setEnabled( false );
    this->verticalHeader()->setVisible( false );
    setSizeAdjustPolicy( QAbstractScrollArea::AdjustToContents );
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    setSelectionBehavior( QAbstractItemView::SelectRows );
    this->setGridStyle( Qt::PenStyle::NoPen );
}

void RemoteFileTableView::dragLeaveEvent(QDragLeaveEvent *e)
{
    DBGLOG << "Action: " << e->type();
    e->accept();
}

void RemoteFileTableView::dragEnterEvent(QDragEnterEvent *e)
{
    DBGLOG << "Drag enter event: " << e->type();
    e->accept();
}

void RemoteFileTableView::dropEvent(QDropEvent *e)
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
            this->selectRow( index.row() );
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

void RemoteFileTableView::dragMoveEvent(QDragMoveEvent *e)
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
            this->selectRow( index.row() );
            destinationListing = model->getDirectoryListingForIndex( index );
        }else
        {
            this->clearSelection();
        }
    }
}

void RemoteFileTableView::startDrag( Qt::DropActions supportedActions )
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
        connect( newMimeData, &RemoteFileMimeData::dropHappenedSignal, this, &RemoteFileTableView::downloadViaDownloadDialogSignal );
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

void RemoteFileTableView::mouseMoveEvent(QMouseEvent *e)
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
    QTableView::mouseMoveEvent( e );
}

void RemoteFileTableView::mouseReleaseEvent(QMouseEvent *e )
{
    DBGLOG << "MouseReleaseEvent: " << e->type();
    if( ! (e->buttons() | Qt::LeftButton ) )
    {
        DBGLOG << "Mouse event: Left mouse button did something.";
    }
    QTableView::mouseReleaseEvent( e );
}

//void RemoteFileTableView::leaveEvent(QEvent *event)
//{
//    DBGLOG << "Leave event.";

//    //If the mouse button is down and an item is selected start the drag timer
//    if( MouseEventFilterSingleton::getInstance()->isLeftMouseButtonDown() &&
//            this->getSelectedItems().count() > 0 )
//    {
//        DBGLOG << "Drop timer started.";
//        m_DropTimer.start( 10 );
//    }

//    QTableView::leaveEvent( event );
//}

QList<QSharedPointer<DirectoryListing> > RemoteFileTableView::getSelectedItems()
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

void RemoteFileTableView::setDownloadDialog(QSharedPointer<DialogDownloadFile> dialog)
{
    m_DownloadDialog = dialog;
}

void RemoteFileTableView::setUploadDialog(QSharedPointer<DialogUploadFile> dialog)
{
    m_UploadDialog = dialog;
}

void RemoteFileTableView::setSettings(QSharedPointer<QSettings> settings)
{
    m_Settings = settings;
}

void RemoteFileTableView::onItemDoubleClicked( const QModelIndex &index )
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

void RemoteFileTableView::dropTimerTimeoutSlot()
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
