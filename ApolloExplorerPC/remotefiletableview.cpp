#include "remotefiletableview.h"
#include "remotefiletablemodel.h"
#include "qdragremote.h"

#include <QDrag>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QHeaderView>
#include <QDir>

#define DEBUG 1
#include "AEUtils.h"

RemoteFileTableView::RemoteFileTableView( QWidget *parent ) :
    QTableView( parent )
{
    //QTableView( parent );

    this->setDragEnabled( true );
    //this->setDragDropMode( DragDrop );
    this->setAcceptDrops( true );

    //Signals and slots
    connect( this, &RemoteFileTableView::doubleClicked, this, &RemoteFileTableView::onItemDoubleClicked );

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
    DBGLOG << "Something happened.";

#if 1
    //Set up the mimedata
    RemoteFileMimeData* newMimeData = new RemoteFileMimeData();
    newMimeData->setDownloadDialog( m_DownloadDialog );
    //newMimeData->setTempFilePath( QDir::tempPath() + "/" );
    newMimeData->setAction( Qt::MoveAction );

    //Get the list of selected items
    auto selectedItems = this->selectedIndexes();
    for( auto item = selectedItems.begin(); item != selectedItems.end(); item++ )
    {
        RemoteFileTableModel *model = dynamic_cast<RemoteFileTableModel*>( this->model() );
        QSharedPointer<DirectoryListing> listing = model->getDirectoryListingForIndex( *item );
        if( !listing.isNull() )
        {
            DBGLOG << "Item at row " << item->row() << ": " << listing->Name();
            newMimeData->addRemotePath( listing );
        }
        else
            DBGLOG << "Item at row " << item->row() << ": NONAME";
    }

    //Create the new drag action
    QDragRemote *drag = new QDragRemote( this );
    drag->setMimeData( newMimeData );
    drag->exec( Qt::MoveAction|Qt::CopyAction, Qt::MoveAction );
    m_QDragList.append( drag );
    DBGLOG <<"Currentl drage queue is: " << m_QDragList.size();
#endif
}

void RemoteFileTableView::mouseMoveEvent(QMouseEvent *e)
{
    DBGLOG << "Mouse event: " << e->type();
    if( ! (e->buttons() | Qt::LeftButton ) )
    {
        DBGLOG << "Mouse event: Left mouse button did something.";
    }
    QTableView::mouseMoveEvent( e );
}

void RemoteFileTableView::mouseReleaseEvent(QMouseEvent *e )
{
    DBGLOG << "Mouse event: " << e->type();
    if( ! (e->buttons() | Qt::LeftButton ) )
    {
        DBGLOG << "Mouse event: Left mouse button did something.";
    }
    QTableView::mouseReleaseEvent( e );
}

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
