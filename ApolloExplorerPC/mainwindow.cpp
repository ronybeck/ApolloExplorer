#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "aboutdialog.h"

#include <QDebug>
#include <QtEndian>
#include <QFileInfo>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QMenu>
#include <QHeaderView>
#include <QDialog>
#include <QRegExp>
#include "messagepool.h"
#include "dialogfileinfo.h"

#include "AEUtils.h"


#define PATH_ROLE 1
#define TYPE_ROLE 2
#define DIR_ENTRY_ROLE 3

#define LOCK QMutexLocker locker( &m_Mutex )
#define UNLOCK locker.unlock()
#define RELOCK locker.relock()

#define REPLACE_ME 0

MainWindow::MainWindow( QSharedPointer<QSettings> settings, QSharedPointer<AmigaHost> amigaHost, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),
    m_ProtocolHandler( ),
    m_DirectoryListings(),
    m_Volumes( ),
    m_AcknowledgeState( ProtocolHandler::AS_Unknown ),
    m_ReconnectTimer( this ),
    m_VolumeRefreshTimer( this ),
    m_AmigaHost( amigaHost ),
    m_FileTableView( nullptr ),
    m_FileTableModel( nullptr ),
    m_Settings( settings ),
    m_DialogPreferences( m_Settings ),
    m_ConfirmWindowClose( true ),
    m_HideInfoFiles( true ),
    m_ShowFileSizes( false ),
    m_ViewType( VIEW_LIST ),
    m_IconCache( this ),
    m_IncomingByteCount( ),
    m_OutgoingByteCount( ),
    m_ThroughputTimer( ),
    //m_OpenCLIHereAction( "Open CLI" ),
    m_Mutex( QMutex::Recursive ),
    m_AbortDeleteRequested( false )
{
    ui->setupUi(this);

    //Setup the host information in the window
    ui->lineEditServerAddress->setText( m_AmigaHost->Address().toString() );
    setWindowTitle( "File browser " + m_AmigaHost->Name() + "(" + m_AmigaHost->Address().toString() + ")" );
    ui->groupBoxServerAddress->hide();

    //Restore the last window position
    m_Settings->beginGroup( SETTINGS_HOSTS );
    m_Settings->beginGroup( m_AmigaHost->Name() );
    this->resize(
                m_Settings->value( SETTINGS_WINDOW_WIDTH, size().width() ).toInt(),
                m_Settings->value( SETTINGS_WINDOW_HEIGHT, size().height() ).toInt()
                );
    this->move(
                m_Settings->value( SETTINGS_WINDOW_POSX, pos().x() ).toInt(),
                m_Settings->value( SETTINGS_WINDOW_POSY, pos().y() ).toInt()
                );
    m_Settings->endGroup();
    m_Settings->endGroup();

    //Setup the dialogs
    m_DialogDownloadFile = QSharedPointer<DialogDownloadFile>( new DialogDownloadFile() );
    m_DialogUploadFile = QSharedPointer<DialogUploadFile>( new DialogUploadFile() );

    //Setup the custom views
    m_FileTableView = new RemoteFileTableView( this );
    m_FileTableView->setDownloadDialog( m_DialogDownloadFile );
    m_FileTableView->setUploadDialog( m_DialogUploadFile );
    m_FileTableView->setSettings( m_Settings );
    m_FileTableView->setEnabled( false );
    ui->verticalLayoutFileBrowser->addWidget( m_FileTableView );


    //setup network signals and slots
    connect( &m_ProtocolHandler, &ProtocolHandler::connectedToHostSignal, this, &MainWindow::onConnectedToHostSlot );
    connect( &m_ProtocolHandler, &ProtocolHandler::disconnectedFromHostSignal, this, &MainWindow::onDisconnectedFromHostSlot );
    connect( this, &MainWindow::connectToHostSignal, &m_ProtocolHandler, &ProtocolHandler::onConnectToHostRequestedSlot );
    connect( this, &MainWindow::disconnectFromHostSignal, &m_ProtocolHandler, &ProtocolHandler::onDisconnectFromHostRequestedSlot );
    connect( this, &MainWindow::disconnectFromHostSignal, &m_ProtocolHandler, &ProtocolHandler::onDisconnectFromHostRequestedSlot );
    connect( this, &MainWindow::sendProtocolMessageSignal, &m_ProtocolHandler, &ProtocolHandler::onSendMessageSlot );
    connect( &m_ProtocolHandler, &ProtocolHandler::serverVersionSignal, this, &MainWindow::onServerVersion );
    connect( &m_ProtocolHandler, &ProtocolHandler::newDirectoryListingSignal, this, &MainWindow::onDirectoryListingUpdateSlot );
    connect( &m_ProtocolHandler, &ProtocolHandler::outgoingByteCountSignal, this, &MainWindow::onOutgoingByteCountUpdateSlot );
    connect( &m_ProtocolHandler, &ProtocolHandler::incomingByteCountSignal, this, &MainWindow::onIncomingByteCountUpdateSlot );
    connect( &m_ProtocolHandler, &ProtocolHandler::volumeListSignal, this, &MainWindow::onVolumeListUpdateSlot );
    connect( this, &MainWindow::getRemoteDirectorySignal, &m_ProtocolHandler, &ProtocolHandler::onGetDirectorySlot );
    connect( this, &MainWindow::createRemoteDirectorySignal, &m_ProtocolHandler, &ProtocolHandler::onMKDirSlot );
    connect( &m_ProtocolHandler, &ProtocolHandler::acknowledgeWithCodeSignal, this, &MainWindow::onAcknowledgeSlot );
    //connect( this, &MainWindow::deleteRemoteDirectorySignal, &m_ProtocolHandler, &ProtocolHandler::onDeleteFileSlot );
    connect( &m_ProtocolHandler, &ProtocolHandler::serverClosedConnectionSignal, this, &MainWindow::onServerClosedConnectionSlot );

    //Setup GUI signals and slots
    connect( ui->pushButtonConnect, &QPushButton::released, this, &MainWindow::onConnectButtonReleasedSlot );
    connect( ui->pushButtonDisconnect, &QPushButton::released, this, &MainWindow::onDisconnectButtonReleasedSlot );
    connect( ui->pushButtonRefresh, &QPushButton::released, this, &MainWindow::onRefreshButtonReleasedSlot );
    connect( ui->pushButtonUp, &QPushButton::released, this, &MainWindow::onUpButtonReleasedSlot );
    //connect( ui->listWidgetFileBrowser, &QListWidget::doubleClicked, this, &MainWindow::onBrowserItemDoubleClickSlot );
    connect( m_FileTableView, &RemoteFileTableView::itemsDoubleClicked, this, &MainWindow::onBrowserItemsDoubleClickedSlot );
    connect( ui->listWidgetDrives, &QListWidget::clicked, this, &MainWindow::onDrivesItemSelectedSlot );
    connect( ui->lineEditPath, &QLineEdit::editingFinished, this, &MainWindow::onPathEditFinishedSlot );
    connect( ui->actionShow_Drives, &QAction::toggled, this, &MainWindow::onShowDrivesToggledSlot );
    connect( ui->actionList_Mode, &QAction::toggled, this, &MainWindow::onListModeToggledSlot );
    connect( ui->actionShow_Info_Files, &QAction::toggled, this, &MainWindow::onShowInfoFilesToggledSlot );
    connect( ui->actionSettings, &QAction::triggered, this, &MainWindow::onSettingsMenuItemClickedSlot );

    //Upload download slots
    connect( m_DialogUploadFile.get(), &DialogUploadFile::allFilesUploadedSignal, this, &MainWindow::onRefreshButtonReleasedSlot );
    connect( m_DialogDownloadFile.get(), &DialogDownloadFile::incomingBytesSignal, this, &MainWindow::onIncomingByteCountUpdateSlot );
    connect( m_DialogUploadFile.get(), &DialogUploadFile::outgoingBytesSignal, this, &MainWindow::onOutgoingByteCountUpdateSlot );

    //Icon Cache
    connect( &m_IconCache, &IconCache::iconSignal, this, &MainWindow::onIconUpdateSlot );
    connect( this, &MainWindow::retrieveIconSignal, &m_IconCache, &IconCache::retrieveIconSlot );

    //Deletion dialog
    connect( &m_DialogDelete, &DialogDelete::cancelDeletionSignal, this, &MainWindow::onAbortDeletionRequestedSlot );
    connect( &m_DialogDelete, &DialogDelete::deletionCompletedSignal, this, &MainWindow::onRefreshButtonReleasedSlot );
    connect( this, &MainWindow::currentFileBeingDeleted, &m_DialogDelete, &DialogDelete::onCurrentFileBeingDeletedSlot );
    connect( this, &MainWindow::deletionCompletedSignal, &m_DialogDelete, &DialogDelete::onDeletionCompleteSlot );

    //Connect actions
    connect( ui->actionDelete, &QAction::triggered, this, &MainWindow::onDeleteSlot );
    connect( ui->actionAbout, &QAction::triggered, this, &MainWindow::onAboutSlot );
    connect( ui->actionReboot_Amiga, &QAction::triggered, this, &MainWindow::onRebootSlot );
    connect( ui->actionCreate_Directory, &QAction::triggered, this, &MainWindow::onMkdirSlot );

    //Context Menu
    //ui->listWidgetBrowser->addAction( &m_OpenCLIHereAction );
    ui->listWidgetFileBrowser->setContextMenuPolicy(Qt::CustomContextMenu);
    m_FileTableView->setContextMenuPolicy( Qt::CustomContextMenu );

    connect( ui->listWidgetFileBrowser, &QListWidget::customContextMenuRequested, this, &MainWindow::showContextMenu );
    connect( m_FileTableView, &QTableView::customContextMenuRequested, this, &MainWindow::showContextMenu );
    connect( m_FileTableView, &RemoteFileTableView::downloadViaDownloadDialogSignal, this, &MainWindow::onDownloadSelectedSlot );

    //Setup the throughput timer
    connect( &m_ThroughputTimer, &QTimer::timeout, this, &MainWindow::onThroughputTimerExpired );
    m_ThroughputTimer.setInterval( 1000 );
    m_ThroughputTimer.setSingleShot( false );
    m_ThroughputTimer.start();

    //Reconnect timer
    connect( &m_ReconnectTimer, &QTimer::timeout, this, &MainWindow::onReconnectTimerExpired );
    m_ReconnectTimer.setSingleShot( false );
    m_ReconnectTimer.setInterval( 10000 );

    //Volume Refresh timer
    connect( &m_VolumeRefreshTimer, &QTimer::timeout, this, &MainWindow::onRefreshVolumesTimerExpired );
    m_VolumeRefreshTimer.setSingleShot( false );
    m_VolumeRefreshTimer.start( 10000 );

    //Disable the list view
    if( m_ViewType == VIEW_LIST )
    {
        m_FileTableView->show();
        ui->listWidgetFileBrowser->hide();
    }
    else
    {
        m_FileTableView->hide();
        ui->listWidgetFileBrowser->show();
    }
}

MainWindow::~MainWindow()
{  
    //Destroy all the dialogs and disconnect
    m_DialogUploadFile->disconnectFromhost();
    m_DialogDownloadFile->disconnectFromhost();
    m_DialogDelete.disconnectFromhost();
    m_DialogUploadFile->deleteLater();
    m_DialogDownloadFile->deleteLater();

    disconnect( &m_ProtocolHandler, &ProtocolHandler::disconnectedFromHostSignal, this, &MainWindow::onDisconnectedFromHostSlot );
    delete ui;
}

void MainWindow::closeEvent (QCloseEvent *event)
{
    if( m_ConfirmWindowClose )
    {
        QMessageBox::StandardButton resBtn = QMessageBox::question( this, "Browser",
                                                                    tr("Are you sure?\n"),
                                                                    QMessageBox::No | QMessageBox::Yes,
                                                                    QMessageBox::Yes);
        if (resBtn != QMessageBox::Yes) {
            event->ignore();
        } else {
            event->accept();
            emit browserWindowCloseSignal();
        }
    }else
    {
        event->accept();
        emit browserWindowCloseSignal();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event )
{
    QMainWindow::resizeEvent( event );

    //Change the settings
    m_Settings->beginGroup( SETTINGS_HOSTS );
    m_Settings->beginGroup( m_AmigaHost->Name() );
    m_Settings->setValue( SETTINGS_WINDOW_WIDTH, event->size().width() );
    m_Settings->setValue( SETTINGS_WINDOW_HEIGHT, event->size().height() );
    m_Settings->endGroup();
    m_Settings->endGroup();
    m_Settings->sync();
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent( event );

    //Change the settings
    m_Settings->beginGroup( SETTINGS_HOSTS );
    m_Settings->beginGroup( m_AmigaHost->Name() );
    m_Settings->setValue( SETTINGS_WINDOW_POSX, event->pos().x() );
    m_Settings->setValue( SETTINGS_WINDOW_POSY, event->pos().y() );
    m_Settings->endGroup();
    m_Settings->endGroup();
    m_Settings->sync();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if ( e->mimeData()->hasUrls() )
    {
        e->acceptProposedAction();
    }
    //qDebug()  << "Event Type " << e->type();
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *e)
{
    Q_UNUSED( e )
    //qDebug() << "DragLeaveEvent";
}

void MainWindow::dragMoveEvent(QDragMoveEvent *e)
{
    Q_UNUSED( e )
    //qDebug() << "Drag move event";
}

void MainWindow::onConnectButtonReleasedSlot()
{
    //form the QHostAddress
    QString serverAddressString = ui->lineEditServerAddress->text();
    QHostAddress serverAddress( serverAddressString );

    //Get the port number
    quint16 port = ui->spinBoxPort->value();

    //Tigger the connect
    emit connectToHostSignal( serverAddress, port );
}

void MainWindow::onDisconnectButtonReleasedSlot()
{
    //m_DialogDownloadFile->disconnectFromhost();
    //m_DialogUploadFile->disconnectFromhost();
    emit disconnectFromHostSignal();
}

void MainWindow::onShowDrivesToggledSlot(bool enabled)
{
    ui->groupBoxDrives->setVisible( enabled );
}

void MainWindow::onListModeToggledSlot(bool enabled)
{
    Q_UNUSED( enabled )
    m_ViewType = VIEW_LIST;
    if( m_ViewType == VIEW_LIST )
    {
        m_FileTableView->show();
        ui->listWidgetFileBrowser->hide();
        m_ShowFileSizes = true;
    }
    else
    {
        m_FileTableView->hide();
        ui->listWidgetFileBrowser->show();
        m_ShowFileSizes = false;
    }
    updateFilebrowser();
}

void MainWindow::onShowInfoFilesToggledSlot(bool enabled)
{
    LOCK;

    m_HideInfoFiles = !enabled;
    if( m_FileTableModel ) m_FileTableModel->showInfoFiles( enabled );
    updateFilebrowser();
}

void MainWindow::onRefreshButtonReleasedSlot()
{
    LOCK;

    //Get the path we need to check for
    QString selectedPath = ui->lineEditPath->text();

    //If we have an empty path, get the volume list
    //if( selectedPath.length() == 0 )
    {
        //Get the list of volumes as well.
        ProtocolMessage_t *getVolumeList = NewMessage();
        if( getVolumeList )
        {
                //Send the query
                getVolumeList->token = MAGIC_TOKEN;
                getVolumeList->type = PMT_GET_VOLUMES;
                getVolumeList->length = sizeof( ProtocolMessage_t );
                m_ProtocolHandler.onSendMessageSlot( getVolumeList );
                FreeMessage( getVolumeList );
        }

        //IF the browser view is at the drive level, just return now
        if( selectedPath.length() == 0 )
            return;
    }

    QString pathString = ui->lineEditPath->text();
    emit getRemoteDirectorySignal( pathString );
}

#if 0
void MainWindow::onBrowserItemDoubleClickSlot()
{
    LOCK;

    QString entryName = "";
    QString currentPath = ui->lineEditPath->text();

    //Get the selected item
    if( m_ViewType == VIEW_LIST )
    {
#if REPLACE_ME
        QList<QTableWidgetItem*> selectedItems =  ui->tableWidgetFileBrowser->selectedItems();
        if( selectedItems.count() == 0 )    return;
        QTableWidgetItem *item = selectedItems[ 0 ];
        entryName = item->text();
#endif
    }else
    {
        QList<QListWidgetItem *> selectedItems = ui->listWidgetFileBrowser->selectedItems();
        if( selectedItems.count() == 0 )    return;     //Nothing to do then
        QListWidgetItem *item = selectedItems[ 0 ];
        entryName = item->text();
    }


    //set the new path to browse to
    QString newPath = currentPath;
    if( newPath.length() == 0 )
    {
        newPath = entryName;
    }else if( newPath.endsWith( ":" ) )
    {
        newPath += entryName;
    }else if( newPath.endsWith( "/" ) )
    {
        newPath += entryName;
    }else
    {
        newPath += "/" + entryName;
    }

    //If we are opening up a drive, we need to first fetch the drive's contents
    //if we don't have it.  Else we can just display a cached copy
    if( currentPath == "" )
    {
        ui->lineEditPath->setText( newPath );
        if( !m_DirectoryListings.contains( newPath ) )
            emit getRemoteDirectorySignal( newPath );
        else
            updateFilebrowser();
        return;
    }

    //Do we have this entry already?
    QSharedPointer<DirectoryListing> directoryListing = m_DirectoryListings[ currentPath ];
    if( directoryListing.isNull() )
    {
        //we should get this directory then.  Seems we don't have it yet.
        emit getRemoteDirectorySignal( newPath );
        return;
    }
    QSharedPointer<DirectoryListing> directoryEntry = directoryListing->findEntry( entryName );

    //If this is a directory that the user double clicked on
    //then we need open that directory
    if( directoryEntry->Type() == DET_USERDIR )
    {
        //Set a new path
        ui->lineEditPath->setText( directoryEntry->Path() );

        //Do we have this path?
        if( !m_DirectoryListings.contains( newPath ) )
            emit getRemoteDirectorySignal( newPath );       //Nope.  We should get it then
        else
            updateFilebrowser();        //Yep.  Let's display what we have
        return;
    }

    //If this is a file, we should start a file download
    if( directoryEntry->Type() == DET_FILE )
    {
        onDownloadSelectedSlot();
    }
}
#endif

void MainWindow::onBrowserItemsDoubleClickedSlot(QList<QSharedPointer<DirectoryListing> > directoryListings )
{
    LOCK;

    static QString localDirPath = QDir::homePath();

    //Examine the list and see if there is anything there
    if( directoryListings.empty() )
    {
        //Nothing to do!
        WARNLOG << "We got an empty directory listing!  This should never happen";
        return;
    }

    //if we have multiple things selected OR the listing is a file, we should trigger a download
    if( directoryListings.size() > 1 ||
            ( directoryListings.size() == 1 && directoryListings[ 0 ]->Type() == DET_FILE ) )
    {
        //Let's see what the user configured to do on double click
        m_Settings->beginGroup( SETTINGS_BROWSER );
        QString action = m_Settings->value( SETTINGS_BROWSER_DOUBLECLICK_ACTION, SETTINGS_DOWNLOAD ).toString();
        m_Settings->endGroup();

        if( action == SETTINGS_IGNORE )
        {
            DBGLOG << "Ignoring double click";
            return;
        }
        if( action == SETTINGS_OPEN )
        {
            DBGLOG << "Opening files after download.";
            m_DialogDownloadFile->startDownloadAndOpen( directoryListings );
            return;
        }
        if( action == SETTINGS_DOWNLOAD )
        {
            DBGLOG << "Downloading files.";
            //Ask the user where to save the files
            QFileDialog destDialog( this, "Destination for files", localDirPath );
            destDialog.setFileMode( QFileDialog::DirectoryOnly );
            int result = destDialog.exec();
            if( result == 0 )
                return;
            //Get the selected directory
            localDirPath = destDialog.directory().absolutePath();
            DBGLOG << "Downloading to directory " << localDirPath;
            m_DialogDownloadFile->startDownload( directoryListings, localDirPath );
            return;
        }
    }

    //The only case left now is a single directory.
    //In this case, we should just open that directory
    QString selectedPath = directoryListings[ 0 ]->Path();
    ui->lineEditPath->setText( selectedPath );
    if( m_DirectoryListings.contains( selectedPath ) )
    {
        //We have this directory already cached.  Just open it
        updateFilebrowser();
        return;
    }

    //Ok, if we are here, then we didn't have the directory cached yet
    char encodedText[ 128 ];
    convertFromUTF8ToAmigaTextEncoding( selectedPath, encodedText, sizeof( encodedText ) );
    //emit getRemoteDirectorySignal( encodedText );
    emit getRemoteDirectorySignal( selectedPath );
}

void MainWindow::onDrivesItemSelectedSlot()
{
    //Get the selected item
    QList<QListWidgetItem *> selectedItems = ui->listWidgetDrives->selectedItems();
    if( selectedItems.count() == 0 )    return;     //Nothing to do then
    QListWidgetItem *item = selectedItems[ 0 ];
    QString entryName = item->text();

    ui->lineEditPath->setText( entryName );
    char encodedText[128];
    convertFromUTF8ToAmigaTextEncoding( entryName, encodedText, sizeof( encodedText ) );
    //emit getRemoteDirectorySignal( encodedText );
    emit getRemoteDirectorySignal( entryName );
    //onRefreshButtonReleasedSlot();
    return;
}

void MainWindow::onUpButtonReleasedSlot()
{
    LOCK;
    QString path = ui->lineEditPath->text();

    //Are we at the top already?
    if( path.endsWith( ':' ) )
    {
        ui->lineEditPath->setText( "" );
    }else if( !path.contains( '/' ) )
    {
        path.remove( path.lastIndexOf( ':' ) + 1, path.length() );
        ui->lineEditPath->setText( path );
    }else
    {
        //Go up a directory
        path.remove( path.lastIndexOf( '/' ), path.length() );
        ui->lineEditPath->setText( path );
    }

    //Update browser
    updateFilebrowser();
}

void MainWindow::onPathEditFinishedSlot()
{
    //Cut the trailing '/' from the path
    QString path = ui->lineEditPath->text();

    if( path.endsWith( '/' ) )
    {
        path.chop( 1 );
        ui->lineEditPath->setText( path );
    }
}

void MainWindow::showContextMenu(QPoint pos )
{
    // Handle global position
    QPoint globalPos = ui->listWidgetFileBrowser->mapToGlobal( pos );

    // Create menu and insert some actions
    QMenu myMenu;
    myMenu.addAction( "Reboot", this, &MainWindow::onRebootSlot );
    //myMenu.addAction( "Run CLI Here", this, &MainWindow::onRunCLIHereSlot );
    myMenu.addSeparator();
    myMenu.addAction( "Delete Files", this, &MainWindow::onDeleteSlot );
    myMenu.addSeparator();
    myMenu.addAction( "Make Directory", this, &MainWindow::onMkdirSlot );
    myMenu.addAction( "Rename", this, &MainWindow::onRenameSlot );
    myMenu.addAction( "Download Files", this, &MainWindow::onDownloadSelectedSlot );
    myMenu.addAction( "Information", this, &MainWindow::onInformationSelectedSlot );

    //If we are currently downloading, we should turn these options off
    if( m_DialogDownloadFile->isCurrentlyDownloading() )
    {
        myMenu.actions().at( 5 )->setEnabled( false );  //Download
        myMenu.actions().at( 0 )->setEnabled( false );  //Reboot
    }

    // Show context menu at handling position
    myMenu.exec( globalPos );
}

#if 0
void MainWindow::onSetHostSlot(QHostAddress host)
{
    ui->lineEditServerAddress->setText( host.toString() );
    setWindowTitle( "File browser " + host.toString() );
    ui->groupBoxServerAddress->hide();
}
#endif

void MainWindow::onAbortDeletionRequestedSlot()
{
    m_AbortDeleteRequested = true;
}

void MainWindow::onIconUpdateSlot(QString filePath, QSharedPointer<AmigaInfoFile> icon)
{
    //First let's detirmin if the this is for a volume or for a file/directory path
    if( filePath.endsWith("Disk" ) || filePath.endsWith( "Disk.info" ) )
    {
        //Get the volume name
        QString volumeName = filePath;
        volumeName = volumeName.left( volumeName.indexOf( ':' ) );

        //This is clearly a volume.  Find the volume and update the icon
        QListIterator<QSharedPointer<DiskVolume>> volumeIter( m_Volumes );
        while( volumeIter.hasNext() )
        {
            QSharedPointer<DiskVolume> entry = volumeIter.next();

            //Is this the volume?
            if( volumeName.compare( entry->getName() ) == 0 )
            {
                //Set the icon
                entry->setAmigaInfoFile( icon );

                //Now refresh the gui
                updateDrivebrowser();
                DBGLOG << "Updated icon for " << volumeName << ".";
                return;
            }
        }
    }

    //Then it must be a file/directory path.  Find this path
    for( auto nextListing = m_DirectoryListings.begin(); nextListing != m_DirectoryListings.end(); nextListing++ )
    {
        //Is this already the path we are looking for?
        if( nextListing.key() == filePath )
        {
            //Set the icon
            nextListing.value()->setAmigaInfoFile( icon );

            //Update the gui
            updateFilebrowser();
            DBGLOG << "Updating icon for " << filePath << ".";
            return;
        }

        //Otherwise let's search the subdirectories until we find the path
        QSharedPointer<DirectoryListing> subListing = nextListing.value()->findEntry( filePath, true );
        if( subListing != nullptr )
        {
            //Set the icon
            subListing->setAmigaInfoFile( icon );

            //Update the gui
            updateFilebrowser();
            DBGLOG << "Updated icon for " << filePath << ".";
            return;
        }
    }

    DBGLOG << "We never found the file path " << filePath << " in our volumes or directory listings.";
}

void MainWindow::onSettingsMenuItemClickedSlot()
{
    m_DialogPreferences.show();
}

void MainWindow::onRunCLIHereSlot()
{
    QHostAddress address( ui->lineEditServerAddress->text() );
    quint16 port = ui->spinBoxPort->value();

    DialogConsole *newConsole = new DialogConsole( address, port, "avil", "work:" );
    newConsole->show();
}

void MainWindow::onMkdirSlot()
{
    LOCK;

    bool ok;
    QString currentPath = ui->lineEditPath->text();
    QString dirPath = ui->lineEditPath->text();
    QString dirName = QInputDialog::getText(this, tr("New Directory"),
                                            tr("Directory Name:"), QLineEdit::Normal,
                                            tr( "New Directory"), &ok);

    //If a name is selected, send a message
    if( ok )
    {
        //Form the path
        if( dirPath.endsWith( ':' ) )
        {
            dirPath.append( dirName );
        }else if( dirPath.endsWith( "/" ) )
        {
            dirPath.append( dirName );
        }else
        {
            dirPath.append( "/" + dirName );
        }

        //Form the message
        ProtocolMessage_MakeDir_t *mkdirMessage = AllocMessage<ProtocolMessage_MakeDir_t>();
        mkdirMessage->header.token = MAGIC_TOKEN;
        mkdirMessage->header.type = PMT_MKDIR;
        mkdirMessage->header.length = sizeof( ProtocolMessage_MakeDir_t ) + dirPath.length();
        strncpy( mkdirMessage->filePath, dirPath.toStdString().c_str(), dirPath.length() + 1 );

        //send the message
        m_ProtocolHandler.sendMessage( mkdirMessage );
        ReleaseMessage( mkdirMessage );
    }

    //Refresh the view
    QThread::msleep( 100 );
    emit getRemoteDirectorySignal( currentPath );
}

void MainWindow::onRebootSlot()
{
    QMessageBox::StandardButton resBtn = QMessageBox::question( this, "Browser",
                                                                tr("Are you sure you want to reboot?\n"),
                                                                QMessageBox::No | QMessageBox::Yes,
                                                                QMessageBox::Yes);
    if (resBtn != QMessageBox::Yes)
    {
        return;
    }

    //Form the message
    ProtocolMessage_Reboot_t *rebootMessage = AllocMessage<ProtocolMessage_Reboot_t>();
    rebootMessage->header.token = MAGIC_TOKEN;
    rebootMessage->header.type = PMT_REBOOT;
    rebootMessage->header.length = sizeof( ProtocolMessage_Reboot_t );


    //send the message
    m_ProtocolHandler.sendMessage( rebootMessage );
    ReleaseMessage( rebootMessage );
}

void MainWindow::onDeleteSlot()
{
    LOCK;

    m_Settings->beginGroup( SETTINGS_BROWSER );
    m_Settings->endGroup();

    QMessageBox msgBox( QMessageBox::Warning, "Delete", "Are you sure you want to delete this files", QMessageBox::Ok|QMessageBox::Cancel );
    if( msgBox.exec() )
    {
        qDebug() << "Deleting.......";
    }else
    {
        qDebug() << "Aborting delete";
        return;
    }

    //Reset the abort deletion indicator
    m_AbortDeleteRequested = false;

    //Open the deletion dialog
    m_DialogDelete.show();

    //Get the list of files selected
    QList<QSharedPointer<DirectoryListing>> directoryListing;
    QString currentDir = ui->lineEditPath->text();
    if( m_ViewType == VIEW_LIST )
    {
        directoryListing = m_FileTableView->getSelectedItems();
    }else
    {
//Replace this here for icon mode
    }

    m_DialogDelete.onDeleteRemotePathsSlot( directoryListing );
}

void MainWindow::onAboutSlot()
{
    m_AboutDialog.show();
}

void MainWindow::onRenameSlot()
{
    LOCK;

    //Get the list of files selected
    QList<QSharedPointer<DirectoryListing>> directoryListing;
    QString currentDir = ui->lineEditPath->text();
    if( m_ViewType == VIEW_LIST )
    {
        directoryListing = m_FileTableView->getSelectedItems();
        if( directoryListing.size() < 1 )   return;

        QInputDialog renameDialog( this );
        renameDialog.setInputMode( QInputDialog::TextInput );
        renameDialog.setTextValue( directoryListing[ 0 ]->Name() );
        renameDialog.setLabelText( "Enter a new name for the file." );
        renameDialog.exec();

        if( renameDialog.result() == QDialog::Accepted )
        {
            DBGLOG << "New name is " << renameDialog.textValue();
            QString oldPathName = directoryListing[ 0 ]->Path();
            directoryListing[ 0 ]->setName( renameDialog.textValue() );
            QString newPathName = directoryListing[ 0 ]->Path();

            //Tell the server to rename the
            m_ProtocolHandler.onRenameFileSlot( oldPathName, newPathName );
        }
    }else
    {
//Replace this here for icon mode
    }

    //Refresh the view
    emit getRemoteDirectorySignal( currentDir );

}

void MainWindow::onDownloadSelectedSlot()
{
    LOCK;

    static QString localDirPath = QDir::homePath();
    QList<QSharedPointer<DirectoryListing>> remotePaths;

    //Get the list of files selected
    if( m_ViewType == VIEW_LIST )
    {
        remotePaths = m_FileTableView->getSelectedItems();
    }else
    {
    //Do something here
    }

    //Ask the user where to save the files
    QFileDialog destDialog( this, "Destination for files", localDirPath );
    destDialog.setFileMode( QFileDialog::DirectoryOnly );
    int result = destDialog.exec();
    if( result == 0 )
        return;

    //Get the destination path the user selected
    QStringList selectedDirs = destDialog.selectedFiles();
    if( selectedDirs.count() < 1 )
    {
        QMessageBox errorBox( QMessageBox::Critical, "Error creating local directory", "A local destination wasn't selected.", QMessageBox::Ok );
        errorBox.exec();
        return;
    }
    localDirPath = selectedDirs.at( 0 );
    QString currentDir = ui->lineEditPath->text();

    m_DialogDownloadFile->startDownload( remotePaths, localDirPath );
}

void MainWindow::onInformationSelectedSlot()
{
    LOCK;

    static QString localDirPath = QDir::homePath();
    QList<QSharedPointer<DirectoryListing>> remotePaths;

    //Get the list of files selected
    if( m_ViewType == VIEW_LIST )
    {
        remotePaths = m_FileTableView->getSelectedItems();
    }else
    {
        //Do something here
    }

    //For each of the selected paths, open a info dialog
    for( QList<QSharedPointer<DirectoryListing>>::Iterator iter = remotePaths.begin(); iter != remotePaths.end(); iter++ )
    {
        //Get the next entry
        QSharedPointer<DirectoryListing> directoryListing = *iter;

        //Create the dialog
        DialogFileInfo dialog( directoryListing, this );
        dialog.exec();
    }

}

void MainWindow::onConnectedToHostSlot()
{
    //Disable the reconnect timer
    m_ReconnectTimer.stop();

    //Set the gui states accordingly
    ui->pushButtonConnect->setEnabled( false );
    ui->pushButtonDisconnect->setEnabled( true );
    ui->lineEditServerAddress->setEnabled( false );
    ui->spinBoxPort->setEnabled( false );
    ui->groupBoxFileBrowser->setEnabled( true );

    //Connect the download Agent
    QHostAddress serverAddress( ui->lineEditServerAddress->text() );
    quint16 port = ui->spinBoxPort->value();
    m_DialogDownloadFile->connectToHost( serverAddress, port );

    //Connect the upload agent
    m_DialogUploadFile->connectToHost( serverAddress, port );

    //Connect the delete agent
    m_DialogDelete.connectToHost( serverAddress, port );

    //Connect the icon cache
    m_IconCache.onConnectToHostSlot( serverAddress, port );

    //Enable the gui
    ui->listWidgetFileBrowser->setEnabled( true );
    m_FileTableView->setEnabled( true );
    ui->listWidgetDrives->setEnabled( true );
    ui->lineEditPath->setEnabled( true );

    //Get the list of volumes from the sever.
    ProtocolMessage_GetVolumeList_t *getVolumeList = AllocMessage<ProtocolMessage_GetVolumeList_t>();
    if( getVolumeList )
    {
            //Send the query
            getVolumeList->header.token = MAGIC_TOKEN;
            getVolumeList->header.type = PMT_GET_VOLUMES;
            getVolumeList->header.length = sizeof( ProtocolMessage_t );
            m_ProtocolHandler.sendMessage( getVolumeList );
            ReleaseMessage( getVolumeList );
    }
}

void MainWindow::onDisconnectedFromHostSlot()
{
    //Set the gui states accordingly
    ui->pushButtonConnect->setEnabled( true );
    ui->pushButtonDisconnect->setEnabled( false );
    ui->lineEditServerAddress->setEnabled( true );
    ui->spinBoxPort->setEnabled( true );
    ui->groupBoxFileBrowser->setEnabled( true );
    ui->labelServerVersion->setText( "Server Version: -" );

    //Disconnect our upload and download dialogs
    //m_DialogDownloadFile->disconnectFromhost();
    //m_DialogUploadFile->disconnectFromhost();
    m_DialogDelete.disconnectFromhost();
    m_IconCache.onDisconnectFromHostSlot();

    //Grey out the gui
    ui->listWidgetFileBrowser->setEnabled( false );
    m_FileTableView->setEnabled( false );
    ui->listWidgetDrives->setEnabled( false );
    ui->lineEditPath->setEnabled( false );
}

void MainWindow::onServerClosedConnectionSlot(QString message)
{
    Q_UNUSED( message )
    //QMessageBox errorBox( QMessageBox::Critical, "Server disconnected.", "The server disconnected with reason: " + message, QMessageBox::Ok );
    //errorBox.exec();
    onDisconnectedFromHostSlot();
}

void MainWindow::onIncomingByteCountUpdateSlot( quint32 bytes )
{
    m_IncomingByteCount += bytes;
    if( bytes > 0 ) m_AmigaHost->setHostRespondedNow();
}

void MainWindow::onOutgoingByteCountUpdateSlot( quint32 bytes )
{
    m_OutgoingByteCount += bytes;
}

void MainWindow::onDeviceLeftSlot()
{
    onDisconnectButtonReleasedSlot();

    //Disconnect our upload and download dialogs
    m_DialogDownloadFile->disconnectFromhost();
    m_DialogUploadFile->disconnectFromhost();

    //Grey out the gui
    ui->listWidgetFileBrowser->setEnabled( false );
    ui->listWidgetDrives->setEnabled( false );
    ui->lineEditPath->setEnabled( false );

    //start the reconnect timer
    //m_ReconnectTimer.start();
}

void MainWindow::onDeviceDiscoveredSlot()
{
    onConnectButtonReleasedSlot();
}

void MainWindow::onServerVersion(quint8 major, quint8 minor, quint8 revision)
{
    //Set the version number in the QUI
    QString version( "Server Version: " + QString::number( major ) + "." + QString::number( minor ) + "." + QString::number( revision ) );
    ui->labelServerVersion->setText( version );
}

void MainWindow::onDirectoryListingUpdateSlot( QSharedPointer<DirectoryListing> newListing )
{
    LOCK;
    //First find the entry in our listing and update it
    m_DirectoryListings[ newListing->Path() ] = newListing;     //TODO: what about subdirectories???????

    //Now update the browser
    QString currentPath = ui->lineEditPath->text();
    if( newListing->Path() == currentPath )
        updateFilebrowser();
    else
    {
        qDebug() << "Got an update for path " << newListing->Path() << " and current path is " << currentPath;
    }

    //We should now get the icons
    QVectorIterator<QSharedPointer<DirectoryListing>> dirIter( newListing->Entries() );
    while( dirIter.hasNext() )
    {
        QSharedPointer<DirectoryListing> nextListing = dirIter.next();

        //Is this an icon file?
        if( nextListing->Path().endsWith( ".info" ) )
        {
            emit retrieveIconSignal( nextListing->Path() );
            //m_IconCache.retrieveIconSlot( nextListing->Path() );
        }
    }
}

void MainWindow::onVolumeListUpdateSlot( QList<QSharedPointer<DiskVolume>> volumes )
{
    m_Volumes = volumes;

    //Update the drive view
    updateDrivebrowser();

    //We should get the icons for the drives
    for( QList<QSharedPointer<DiskVolume>>::Iterator iter = volumes.begin(); iter != volumes.end(); iter++ )
    {
        auto drive = *iter;

        //Form the path to the disk icon
        QString iconPath = drive->getName() + ":Disk.info";

        //Now trigger the retrieval of the icon
        emit retrieveIconSignal( iconPath );
    }

    //We should now check that at least one drive is selected and then fetch the contents of that drive.
    if( ( volumes.size() > 0 ) )
    {
        //Get the first drive in the list
        QString firstDrive = volumes.first()->getName() + ":";

        //If we don't have a drive already selected, select the first one now.
        if( ui->lineEditPath->text() == "" )
        {
            ui->lineEditPath->setText( firstDrive );
            emit getRemoteDirectorySignal( firstDrive );
            return;
        }

        //If the path, doesn't exist, select anotherone now.
        QString drivePath = ui->lineEditPath->text();
        drivePath = drivePath.left( drivePath.indexOf( ":") );
        QList<QSharedPointer<DiskVolume>>::Iterator iter;
        for( iter = volumes.begin();  iter != volumes.end(); iter++ )
        {
            auto drive = *iter;
            if( drive->getName() == drivePath )
                return; //Ok, the drive is listed, we can exist now
        }

        //If we got this far, then the drive listed in the path field is no longer present on the system.
        //We should automatically switch to a new drive
        ui->lineEditPath->setText( firstDrive );
        emit getRemoteDirectorySignal( firstDrive );
    }
}

void MainWindow::onAcknowledgeSlot( quint8 responseCode )
{
    switch( responseCode )
    {
        case 0:
            m_AcknowledgeState = ProtocolHandler::AS_FAILED;
        break;
        case 1:
            m_AcknowledgeState = ProtocolHandler::AS_SUCCESS;
        break;
        default:
            m_AcknowledgeState = ProtocolHandler::AS_Unknown;
        break;
    }


}

void MainWindow::onThroughputTimerExpired()
{
    //Update the counters in the gui
    ui->labelUploadSpeed->setText( "Upload: " + QString::number( m_OutgoingByteCount/1024 ) + "kB/s" );
    ui->labelDownloadSpeed->setText( "Download: " + QString::number( m_IncomingByteCount/1024 ) + "kB/s" );

    //If we have throughput, then the host is alive.
    //But sometimes the uploads block the heartbeat.
    //So lets refresh the alive timer
    if( m_IncomingByteCount > 0 || m_OutgoingByteCount > 0 )
    {
        m_AmigaHost->setHostRespondedNow();
    }

    //Clear the counters
    m_IncomingByteCount = 0;
    m_OutgoingByteCount = 0;
}

void MainWindow::onReconnectTimerExpired()
{
    this->onConnectButtonReleasedSlot();
}

void MainWindow::onRefreshVolumesTimerExpired()
{
    ProtocolMessage_t *getVolumeList = NewMessage();
    if( getVolumeList )
    {
            //Send the query
            getVolumeList->token = MAGIC_TOKEN;
            getVolumeList->type = PMT_GET_VOLUMES;
            getVolumeList->length = sizeof( ProtocolMessage_t );
            m_ProtocolHandler.onSendMessageSlot( getVolumeList );
            FreeMessage( getVolumeList );
    }
}

void MainWindow::updateFilebrowser()
{
    LOCK;

    //Get the current file path
    QString selectedPath = ui->lineEditPath->text();

    //If the path is empty, then show the drives from the server
    if( selectedPath.length() == 0 )
    {
        //If we don't have a list of voluems, get them
        if( m_Volumes.empty() )
        {
            onRefreshButtonReleasedSlot();
            return;
        }


        //Go through the drives
        QListIterator<QSharedPointer<DiskVolume>> iter( m_Volumes );
        while( iter.hasNext() )
        {
            //Get the next volume
            QSharedPointer<DiskVolume> volume = iter.next();
            QPixmap icon( ":/browser/icons/Harddisk_Amiga.png" );

            //Add this to the view
            if( m_ViewType == VIEW_LIST )
            {
#if REPLACE_ME
                //Create the row items
                QTableWidgetItem *iconItem = new QTableWidgetItem( icon, volume );
                QTableWidgetItem *fileSizeItem = new QTableWidgetItem( "-" );

                quint32 rowCount = ui->tableWidgetFileBrowser->rowCount();
                ui->tableWidgetFileBrowser->insertRow( rowCount );
                ui->tableWidgetFileBrowser->setItem( rowCount, 0, iconItem );
                ui->tableWidgetFileBrowser->setItem( rowCount, 1, fileSizeItem );
#endif
            }else
            {
                //Create the entry for our view
                QListWidgetItem *item = new QListWidgetItem( volume->getName() + ":" );

                item->setIcon( icon );
                item->setStatusTip( volume->getName() + ":" );

                ui->listWidgetFileBrowser->addItem( item );
            }
        }

        return;
    }


    //Do we have a cached copy of the current listing?
    if( !m_DirectoryListings.contains( selectedPath ) )
    {
        //We need to request the current path because we don't have it in our cache
        onRefreshButtonReleasedSlot();

        return;
    }

    //Get the current listing
    QSharedPointer<DirectoryListing> listing = m_DirectoryListings[ selectedPath ];
    if( listing.isNull() )
    {
        //we don't have this path yet.  We should get it
        emit getRemoteDirectorySignal( selectedPath );
        return;
    }

    if( m_ViewType == VIEW_LIST )
    {
        m_FileTableView->setEnabled( true );
        if( m_FileTableModel )
        {
            m_FileTableModel->updateListing( m_DirectoryListings[ selectedPath ] );
            //m_FileTableModel->setSettings( m_Settings );
        }
        else
        {
            m_FileTableModel = new RemoteFileTableModel( m_DirectoryListings[ selectedPath ] );
            m_FileTableModel->setSettings( m_Settings);
            m_FileTableModel->showInfoFiles( !m_HideInfoFiles );
            m_FileTableView->setModel( m_FileTableModel );
            connect( m_FileTableView->horizontalHeader(), &QHeaderView::sectionClicked, m_FileTableModel, &RemoteFileTableModel::onHeaderSectionClicked  );
        }
    }

    //List all the directories first
    QVectorIterator<QSharedPointer<DirectoryListing>> dirIter( listing->Entries() );
    while( dirIter.hasNext() )
    {
        //Get the next entry
        QSharedPointer<DirectoryListing> nextListEntry = dirIter.next();

        //If this is a file, ignore it on this pass
        if( nextListEntry->Type() != DET_USERDIR )
            continue;

        //If we have elected to hide info files, we shouldn't show info files
        if( nextListEntry->Name().endsWith( ".info" ) && m_HideInfoFiles )
            continue;

        if( m_ViewType == VIEW_LIST )
        {
#if REPLACE_ME
            //Create the row items
            QTableWidgetItem *iconItem = new QTableWidgetItem( *nextListEntry->Icon(), nextListEntry->Name() );
            QTableWidgetItem *fileSizeItem = new QTableWidgetItem( getSizeAsFormatedString( nextListEntry->Size() ) );


            quint32 rowCount = ui->tableWidgetFileBrowser->rowCount();
            ui->tableWidgetFileBrowser->insertRow( rowCount );
            ui->tableWidgetFileBrowser->setItem( rowCount, 0, iconItem );
            ui->tableWidgetFileBrowser->setItem( rowCount, 1, fileSizeItem );
#endif
        }else
        {
            //Create the entry for our view
            QString label = nextListEntry->Name() + "\n(" + QString::number( nextListEntry->Size()/1024 ) + "KB)";
            QListWidgetItem *item = new QListWidgetItem( nextListEntry->Name() );
            item->setIcon( *nextListEntry->Icon() );
            item->setToolTip( label );

            //Add this to the view
            ui->listWidgetFileBrowser->addItem( item );
        }
    }

    //Now list files
    QVectorIterator<QSharedPointer<DirectoryListing>> fileIter( listing->Entries() );
    while( fileIter.hasNext() )
    {
        //Get the next entry
        QSharedPointer<DirectoryListing> nextListEntry = fileIter.next();

        //If this is a file, ignore it on this pass
        if( nextListEntry->Type() == DET_USERDIR )
            continue;

        //If we have elected to hide info files, we shouldn't show info files
        if( nextListEntry->Name().endsWith( ".info" ) && m_HideInfoFiles )
            continue;

        //Create the entry for our view
        QString fileSizeText = prettyFileSize( nextListEntry->Size() );
        QString fileNameLabel = nextListEntry->Name();
        QString toolTip = nextListEntry->Name() + "\n" + fileSizeText;

        if( m_ViewType == VIEW_LIST )
        {
#if REPLACE_ME
            //Create the row items
            QTableWidgetItem *iconItem = new QTableWidgetItem( *nextListEntry->Icon(), nextListEntry->Name() );
            QTableWidgetItem *fileSizeItem = new QTableWidgetItem( getSizeAsFormatedString( nextListEntry->Size() ) );
            quint32 rowCount = ui->tableWidgetFileBrowser->rowCount();
            ui->tableWidgetFileBrowser->insertRow( rowCount );
            ui->tableWidgetFileBrowser->setItem( rowCount, 0, iconItem );
            ui->tableWidgetFileBrowser->setItem( rowCount, 1, fileSizeItem );
#endif
        }else
        {
            QListWidgetItem *item = new QListWidgetItem( fileNameLabel );
            item->setIcon( *nextListEntry->Icon() );
            item->setToolTip( toolTip );

            //Add this to the view
            ui->listWidgetFileBrowser->addItem( item );
        }
    }
}

void MainWindow::updateDrivebrowser()
{
    //Clear out the existing view
    ui->listWidgetDrives->clear();

    //Go through the drives
    QListIterator<QSharedPointer<DiskVolume>> iter( m_Volumes );
    while( iter.hasNext() )
    {
        //Get the next volume
        QSharedPointer<DiskVolume> volume = iter.next();

        //See if we have an icon for this drive in our cache
        auto cachedIcon = m_IconCache.getIcon( volume->getName() + ":Disk.info" );

        //Create the entry for our view
        QListWidgetItem *item = new QListWidgetItem( volume->getName() + ":" );
        if( cachedIcon.isNull() )
        {
            QPixmap icon( volume->getPixmap() );
            item->setIcon( icon );
        }else
        {
            QPixmap icon = QPixmap::fromImage( cachedIcon->getBestImage1().scaledToHeight( 64, Qt::SmoothTransformation) );
            item->setIcon( icon );
        }
        item->setToolTip( volume->getName() + " - " +
                          prettyFileSize( volume->getUsedInBytes() ) + "/" + prettyFileSize( volume->getSizeInBytes() ) +
                          " used." );

        //Add this to the view
        ui->listWidgetDrives->addItem( item );
    }
}

void MainWindow::deleteDirectoryRecursive(QString remotePath, bool &error)
{
    m_Settings->beginGroup( SETTINGS_BROWSER );
    qint32 deleteDelay = m_Settings->value( SETTINGS_BROWSER_DELAY_BETWEEN_DELETES, 100 ).toInt();
    m_Settings->endGroup();

    //First clearout our cached copy of this directory.
    //We will get a fresh copy
    m_DirectoryListings.remove( remotePath );

    //We are going to wait on the reply for the get Remote directory signal
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    connect( &m_ProtocolHandler, &ProtocolHandler::newDirectoryListingSignal, &loop, &QEventLoop::quit );
    connect( &timer, &QTimer::timeout, &loop, &QEventLoop::quit );
    timer.start( 10000 );    //Wait up to 10 seconds.  Note:  Floppies and CDROMS are SLLLLLOOOOOOWWWWW
    emit getRemoteDirectorySignal( remotePath );

    //Wait a few times incase some parrallel "getDir" operations occur
    while( timer.isActive() )
    {
        if( m_DirectoryListings.contains( remotePath ) )
            break;
        loop.exec();
        qDebug() << "Waiting on getDir( " << remotePath << ")";
    }

    //Did a timeout occur?
    if( !timer.isActive() )
    {
        qDebug() << "Timeout exceeded getting directory " << remotePath;
        QMessageBox errorBox( QMessageBox::Critical, "Timeout getting remote directory", "A timeout occurred while retrieving remote directory" + remotePath, QMessageBox::Ok );
        errorBox.exec();
        error = false;
        return;
    }

    //Did we really get a list of files?
    LOCK;
    if( !m_DirectoryListings.contains( remotePath ) )
    {
        qDebug() << "Seems we didn't get the remote directory " << remotePath;
        QMessageBox errorBox( QMessageBox::Critical, "Failed to get remote directory", "An error occurred while retrieving remote directory" + remotePath, QMessageBox::Ok );
        errorBox.exec();
        error = false;
        return;
    }

    //Now that we have a new listing (hopefully), we can go through the list of files we need to get
    QVector<QSharedPointer<DirectoryListing>> directoryListing = m_DirectoryListings[ remotePath ]->Entries();
    QVectorIterator<QSharedPointer<DirectoryListing>> iter( directoryListing );
    while( iter.hasNext() )
    {
        //Process current events
        QApplication::processEvents();

        //Check if an abort was issued
        if( m_AbortDeleteRequested )
            return;

        //Wait between each delete as to not overwelm the amiga
        QThread::msleep( deleteDelay );

        //Get the next entry
        QSharedPointer<DirectoryListing> listing = iter.next();

        //If this is a file, add it to the file list
        if( listing->Type() == DET_FILE )
        {
            QString nextRemoteFile = remotePath;
            if( remotePath.endsWith( ":") || remotePath.endsWith( "/") )
                nextRemoteFile += listing->Name();
            else
                nextRemoteFile += "/" + listing->Name();

            //Update the deletion dialog
            emit currentFileBeingDeleted( nextRemoteFile );
            QApplication::processEvents();

            //Delete this file
            //emit deleteRemoteDirectorySignal( nextRemoteFile );
            QString errorMessage;
            if( m_ProtocolHandler.deleteFile( nextRemoteFile, errorMessage ) == false )
            {
                qDebug() << "Failed to delete path " << nextRemoteFile;
                QMessageBox errorBox( QMessageBox::Critical, "Failed to delete remote directory", "An error occurred while deleting remote directory" + nextRemoteFile, QMessageBox::Ok );
                errorBox.exec();
                error = true;
                return;
            }

            qDebug() << "Deleting file " << nextRemoteFile;
            continue;
        }

        //If it this is a directory, iterate
        if( listing->Type() == DET_USERDIR )
        {
            QString nextRemotePath = remotePath;
            if( nextRemotePath.endsWith( ":" ) || nextRemotePath.endsWith( "/") )
                nextRemotePath += listing->Name();
            else
                nextRemotePath += "/" + listing->Name();

            //We need to recurse into this directory and do the same operation
            bool errorInSubdir = false;
            deleteDirectoryRecursive( nextRemotePath, errorInSubdir );

            //If we failed for some reason, let's end here.
            if( errorInSubdir )
            {
                error = true;
                return;
            }

            //Otherwise, remove the directory as well.
            //emit deleteRemoteDirectorySignal( nextRemoteFile );
            QString errorMessage;
            if( m_ProtocolHandler.deleteFile( nextRemotePath, errorMessage ) == false )
            {
                qDebug() << "Failed to delete path " << nextRemotePath;
                QMessageBox errorBox( QMessageBox::Critical, "Failed to delete remote directory", "An error occurred while deleting remote directory" + nextRemotePath, QMessageBox::Ok );
                errorBox.exec();
                error = true;
                return;
            }

            qDebug() << "Deleting file " << nextRemotePath;
        }
    }
    return;
}



QList<QPair<QString,QString>> MainWindow::downloadPrepareLocalDirectory( QString localPath, QString remotePath, bool &error )
{
    bool keepWaitingOnPreviousAttempt = false;
    QList<QPair<QString,QString>> files;

    //First off, make sure the local directory exists
    QFileInfo localDirInfo( localPath );
    if( !localDirInfo.exists() )
    {
        QDir localDir;
        if( ! localDir.mkdir( localPath ) )
        {
            qDebug() << "Unable to create " << localPath << " for remote path " << remotePath;
            QMessageBox errorBox( QMessageBox::Critical, "Error creating local directory", "Unable to create direcotry " + localPath, QMessageBox::Ok );
            errorBox.exec();
            error = true;
            return files;
        }
    }

    //Now get a list of the remote files
    //Let's clear out our cached copy of the remote directory and get a fresh copy
    m_DirectoryListings.remove( remotePath );

    wait_for_dir:
    quint32 waitTime = 60000;  //Wait up to 120 seconds.  Note:  This is not over doing it.  I have seen WB take 60 seconds to get a directory list of BeneathASteelSky for the CD32.  We will take longer.
    //Wait on the reply with a timeout
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    connect( &m_ProtocolHandler, &ProtocolHandler::newDirectoryListingSignal, &loop, &QEventLoop::quit );
    connect( &timer, &QTimer::timeout, &loop, &QEventLoop::quit );
    timer.start( waitTime );
    if( !keepWaitingOnPreviousAttempt )
        emit getRemoteDirectorySignal( remotePath );

    //In case multiple parallel getDir() operations are going on, we should wait for the right one
    while( timer.isActive() )
    {
        keepWaitingOnPreviousAttempt = true;
        if( m_DirectoryListings.contains( remotePath ) )
            break;
        loop.exec();
    }

    //Did a timeout occur?
    if( !timer.isActive() )
    {
        waitTime *=2;
        qDebug() << "Timeout exceeded getting directory " << remotePath;
        QMessageBox::StandardButton resBtn = QMessageBox::question( this, "Timeout",
                                                                    tr("A timeout occured getting directory:\n\n") + QString(remotePath ) + tr("\n\nWould you like to keep waiting (") + QString::number( waitTime/1000 ) + tr(" seconds)?"),
                                                                    QMessageBox::No | QMessageBox::Yes,
                                                                    QMessageBox::Yes);
        if (resBtn != QMessageBox::Yes)
        {
            return files;
        }else
        {
            //Ok, the user wants to keep waiting.  Let's be sure
            if( !m_DirectoryListings.contains( remotePath ) )
                goto wait_for_dir;
        }
    }

    //Did we really get a list of files?
    if( !m_DirectoryListings.contains( remotePath ) )
    {
        qDebug() << "Seems we didn't get the remote directory " << remotePath;
        QMessageBox errorBox( QMessageBox::Critical, "Failed to get remote directory", "An error occurred while retrieving remote directory" + remotePath, QMessageBox::Ok );
        errorBox.exec();
        error = false;
        return files;
    }

    //Now that we have a new listing (hopefully), we can go through the list of files we need to get
    QVector<QSharedPointer<DirectoryListing>> directoryListing = m_DirectoryListings[ remotePath ]->Entries();
    QVectorIterator<QSharedPointer<DirectoryListing>> iter( directoryListing );
    while( iter.hasNext() )
    {
        //Get the next entry
        QSharedPointer<DirectoryListing> listing = iter.next();

        //If this is a file, add it to the file list
        if( listing->Type() == DET_FILE )
        {
            QString nextLocalFile = localPath + "/" + listing->Name();
            QString nextRemoteFile = remotePath;
            if( remotePath.endsWith( ":") || remotePath.endsWith( "/") )
                nextRemoteFile += listing->Name();
            else
                nextRemoteFile += "/" + listing->Name();
            files.push_back( QPair<QString,QString>( nextLocalFile, nextRemoteFile ) );
            //qDebug() << "Download: Adding file " << nextLocalFile << " <------ " << nextRemoteFile << " to the list";
            continue;
        }

        //If it this is a directory, iterate
        if( listing->Type() == DET_USERDIR )
        {
            QString nextLocalPath = localPath + "/" + listing->Name();
            QString nextRemotePath = remotePath;
            if( nextRemotePath.endsWith( ":" ) || nextRemotePath.endsWith( "/") )
                nextRemotePath += listing->Name();
            else
                nextRemotePath += "/" + listing->Name();

            //Get the file list from a directory deeper
            bool errorInSubdir = false;
            auto subdirFiles = downloadPrepareLocalDirectory( nextLocalPath, nextRemotePath, errorInSubdir );
            if( errorInSubdir )
            {
                error = true;
                return files;
            }

            //Add this to our currentlist
            if( subdirFiles.count() > 0 ) files.append( subdirFiles );
        }
    }

    return files;
}

QList<QPair<QString,QString>>  MainWindow::uploadPrepareRemoteDirctory( QString localPath, QString remotePath, bool &error )
{
    QList<QPair<QString,QString>> files;

    //Create the remote directory first
    m_AcknowledgeState = ProtocolHandler::AS_Unknown;
    emit createRemoteDirectorySignal( remotePath );

    //Wait on the reply with a timeout
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    connect( &m_ProtocolHandler, &ProtocolHandler::acknowledgeSignal, &loop, &QEventLoop::quit );
    connect( &timer, &QTimer::timeout, &loop, &QEventLoop::quit );
    timer.start( 10000 );    //Wait up to 10 seconds.  Note:  Floppies and CDROMS are SLLLLLOOOOOOWWWWW
    loop.exec();

    //Did we timeout?  Did the dir creation succeed?
    if( !timer.isActive() || m_AcknowledgeState != ProtocolHandler::AS_SUCCESS )
    {
        qDebug() << "Timeout exceeded creating directory " << remotePath;
        QMessageBox errorBox( QMessageBox::Critical, "Timeout creating remote directory", "A timeout occurred while creating remote directory" + remotePath, QMessageBox::Ok );
        errorBox.exec();
        error = true;
        return files;
    }

    //Open the local path and check all the directories that will need to exist remotely
    QDir localRootDir( localPath );
    QStringList subdirectories = localRootDir.entryList();
    QStringListIterator iter( subdirectories );
    while( iter.hasNext() )
    {
        QString entryName = iter.next();

        //Form the next directory level names
        QString nextLocalPath = localPath + "/" + entryName;
        QString nextRemotePath = remotePath;
        if( nextRemotePath.endsWith( ":" ) || nextRemotePath.endsWith( "/" ) )
            nextRemotePath += entryName;
        else
            nextRemotePath += "/" + entryName;

        //If it is a file, just add it to the list
        QFileInfo fileInfo( nextLocalPath );
        if( fileInfo.isFile() )
        {
            //qDebug() << "Adding file " << nextLocalPath << " ------> " << nextRemotePath << " to upload list.";
            QPair<QString, QString> filePair( nextLocalPath, nextRemotePath );
            files.push_back( filePair );
            continue;
        }

        //A directory requires exploring
        if( fileInfo.isDir() )
        {
            //For unix machines, we want to skip some names
            if( entryName == "." || entryName == ".." )
                continue;

            //Create the this on the remote
            bool errorOnSubdir = false;
            auto subdirFiles = uploadPrepareRemoteDirctory( nextLocalPath, nextRemotePath, errorOnSubdir );
            if( errorOnSubdir )
            {
                error = true;
                return files;
            }

            //Add the files we find bellow
            files.append( subdirFiles );

            continue;
        }
    }

    //If we made it this far, we must have done everything right
    error = false;
    return files;
}

bool MainWindow::ConfirmWindowClose() const
{
    return m_ConfirmWindowClose;
}

void MainWindow::setConfirmWindowClose(bool newConfirmWindowClose)
{
    m_ConfirmWindowClose = newConfirmWindowClose;
}

