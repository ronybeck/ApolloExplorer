#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QtEndian>
#include <QFileInfo>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QTableWidgetItem>
#include  <QMenu>

#include "messagepool.h"
#include "VNetPCUtils.h"

#define PATH_ROLE 1
#define TYPE_ROLE 2
#define DIR_ENTRY_ROLE 3

#define LOCK QMutexLocker locker( &m_Mutex )
#define UNLOCK locker.unlock()
#define RELOCK locker.relock()

constexpr const quint64 GigaByte = 1024 * 1024 * 1024;
constexpr const quint64 MegaByte = 1024 * 1024;
constexpr const quint64 KiloByte = 1024;

static QString getSizeAsFormatedString( quint64 size )
{

    if( size > GigaByte )
    {
        return QString( QString::number( size / GigaByte ) + " GBytes" );
    }

    if( size > MegaByte )
    {
        return QString( QString::number( size / MegaByte ) + " MBytes" );
    }

    if( size > KiloByte )
    {
        return QString( QString::number( size / KiloByte ) + " kBytes");
    }

    return QString( QString::number( size / KiloByte ) + " Bytes");
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),
      m_ProtocolHandler( ),
      m_DirectoryListings(),
      m_Volumes( ),
      m_AcknowledgeState( AS_Unknown ),
      m_ReconnectTimer( this ),
      m_ConfirmWindowClose( true ),
      m_HideInfoFiles( true ),
      m_ShowFileSizes( false ),
      m_ListViewEnabled( false ),
      m_DialogDownloadFile( ),
      m_DialogUploadFile( ),
      m_IncomingByteCount( ),
      m_OutgoingByteCount( ),
      m_ThroughputTimer( ),
      //m_OpenCLIHereAction( "Open CLI" ),
      m_Mutex( QMutex::Recursive ),
      m_AbortDeleteRequested( false )
{
    ui->setupUi(this);

    setAcceptDrops(true);


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

    //Setup GUI signals and slots
    connect( ui->pushButtonConnect, &QPushButton::released, this, &MainWindow::onConnectButtonReleasedSlot );
    connect( ui->pushButtonDisconnect, &QPushButton::released, this, &MainWindow::onDisconnectButtonReleasedSlot );
    connect( ui->pushButtonRefresh, &QPushButton::released, this, &MainWindow::onRefreshButtonReleasedSlot );
    connect( ui->pushButtonUp, &QPushButton::released, this, &MainWindow::onUpButtonReleasedSlot );
    connect( ui->listWidgetFileBrowser, &QListWidget::doubleClicked, this, &MainWindow::onBrowserItemDoubleClickSlot );
    connect( ui->tableWidgetFileBrowser, &QListWidget::doubleClicked, this, &MainWindow::onBrowserItemDoubleClickSlot );
    connect( ui->listWidgetDrives, &QListWidget::clicked, this, &MainWindow::onDrivesItemSelectedSlot );
    connect( ui->lineEditPath, &QLineEdit::editingFinished, this, &MainWindow::onPathEditFinishedSlot );
    connect( ui->actionShow_Drives, &QAction::toggled, this, &MainWindow::onShowDrivesToggledSlot );
    connect( ui->actionList_Mode, &QAction::toggled, this, &MainWindow::onListModeToggledSlot );
    connect( ui->actionShow_Info_Files, &QAction::toggled, this, &MainWindow::onShowInfoFilesToggledSlot );

    //Upload download slots
    connect( &m_DialogUploadFile, &DialogUploadFile::uploadCompletedSignal, this, &MainWindow::onRefreshButtonReleasedSlot );

    //Deletion dialog
    connect( &m_DialogDelete, &DialogDelete::cancelDeletionSignal, this, &MainWindow::onAbortDeletionRequestedSlot );
    connect( this, &MainWindow::currentFileBeingDeleted, &m_DialogDelete, &DialogDelete::onCurrentFileBeingDeletedSlot );
    connect( this, &MainWindow::deletionCompletedSignal, &m_DialogDelete, &DialogDelete::onDeletionCompleteSlot );

    //Context Menu
    //ui->listWidgetBrowser->addAction( &m_OpenCLIHereAction );
    ui->listWidgetFileBrowser->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tableWidgetFileBrowser->setContextMenuPolicy(Qt::CustomContextMenu);
    connect( ui->listWidgetFileBrowser, &QListWidget::customContextMenuRequested, this, &MainWindow::showContextMenu );
    connect( ui->tableWidgetFileBrowser, &QListWidget::customContextMenuRequested, this, &MainWindow::showContextMenu );

    //Setup the throughput timer
    connect( &m_ThroughputTimer, &QTimer::timeout, this, &MainWindow::onThroughputTimerExpired );
    m_ThroughputTimer.setInterval( 1000 );
    m_ThroughputTimer.setSingleShot( false );
    m_ThroughputTimer.start();

    //Reconnect timer
    connect( &m_ReconnectTimer, &QTimer::timeout, this, &MainWindow::onReconnectTimerExpired );
    m_ReconnectTimer.setSingleShot( false );
    m_ReconnectTimer.setInterval( 10000 );

    //Disable the list view
    if( m_ListViewEnabled )
    {
        ui->tableWidgetFileBrowser->show();
        ui->listWidgetFileBrowser->hide();
    }
    else
    {
        ui->tableWidgetFileBrowser->hide();
        ui->listWidgetFileBrowser->show();
    }

    this->setWindowTitle( "File Browser" );
}

MainWindow::~MainWindow()
{
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

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if ( e->mimeData()->hasUrls() )
    {
        e->acceptProposedAction();
    }
    //qDebug()  << "Event Type " << e->type();
}

void MainWindow::dropEvent(QDropEvent *e)
{
    QMessageBox msgBox( this );
    msgBox.setWindowTitle( "Please Wait" );
    msgBox.setText( "We must prepare the directories on the server.\nThis might take some time.\nEspecially if it is a floppy drive." );
    msgBox.setIcon( QMessageBox::Information );
    msgBox.setStandardButtons( QMessageBox::NoButton );
    msgBox.show();


    QList<QPair<QString,QString>> uploadList;
    foreach (const QUrl &url, e->mimeData()->urls())
    {
        //Local file path
        QString localFilename = url.toLocalFile();
        qDebug() << "Dropped file:" << localFilename;

        //Form the remote path
        QFileInfo localFile( localFilename );
        QString remoteFilename = ui->lineEditPath->text();
        if( !ui->lineEditPath->text().endsWith( '/' ) && !ui->lineEditPath->text().endsWith( ':' ) )
        {
            remoteFilename.append( "/" );
        }
        remoteFilename.append( localFile.fileName() );

        //On directories, we need to search
        if( localFile.isDir() )
        {
            bool errorOnSubDir = false;
            auto subdirFiles = uploadPrepareRemoteDirctory( localFilename, remoteFilename, errorOnSubDir );
            if( errorOnSubDir )
            {
                QMessageBox errorBox( QMessageBox::Critical, "Server error", "Some error occurred at the server end.  Unable to continue.", QMessageBox::Ok );
                errorBox.exec();
                return;
            }

            //All is well, add these files to our list
            uploadList.append( subdirFiles );

            //We don't want to add this to the list
            continue;
        }


        //Form the next entry in our list
        uploadList.push_back( QPair<QString,QString>(localFilename, remoteFilename) );
    }
    msgBox.hide();

    //Start the upload
    m_DialogUploadFile.show();
    m_DialogUploadFile.startUpload( uploadList );
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *e)
{
    qDebug() << "DragLeaveEvent";
}

void MainWindow::dragMoveEvent(QDragMoveEvent *e)
{
    qDebug() << "Drag move event";
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
    m_DialogDownloadFile.disconnectFromhost();
    m_DialogUploadFile.disconnectFromhost();
    emit disconnectFromHostSignal();
}

void MainWindow::onShowDrivesToggledSlot(bool enabled)
{
    ui->groupBoxDrives->setVisible( enabled );
}

void MainWindow::onListModeToggledSlot(bool enabled)
{
    m_ListViewEnabled = enabled;
    if( m_ListViewEnabled )
    {
        ui->tableWidgetFileBrowser->show();
        ui->listWidgetFileBrowser->hide();
        m_ShowFileSizes = true;
    }
    else
    {
        ui->tableWidgetFileBrowser->hide();
        ui->listWidgetFileBrowser->show();
        m_ShowFileSizes = false;
    }
    updateFilebrowser();
}

void MainWindow::onShowInfoFilesToggledSlot(bool enabled)
{
    m_HideInfoFiles = !enabled;
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

void MainWindow::onBrowserItemDoubleClickSlot()
{
    LOCK;

    QString entryName = "";
    QString currentPath = ui->lineEditPath->text();

    //Get the selected item
    if( m_ListViewEnabled )
    {
        QList<QTableWidgetItem*> selectedItems = ui->tableWidgetFileBrowser->selectedItems();
        if( selectedItems.count() == 0 )    return;
        QTableWidgetItem *item = selectedItems[ 0 ];
        entryName = item->text();
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

void MainWindow::onDrivesItemSelectedSlot()
{
    //Get the selected item
    QList<QListWidgetItem *> selectedItems = ui->listWidgetDrives->selectedItems();
    if( selectedItems.count() == 0 )    return;     //Nothing to do then
    QListWidgetItem *item = selectedItems[ 0 ];
    QString entryName = item->text();

    ui->lineEditPath->setText( entryName );
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
    myMenu.addAction( "Download Files", this, &MainWindow::onDownloadSelectedSlot );

    //If we are currently downloading, we should turn these options off
    if( m_DialogDownloadFile.isCurrentlyDownloading() )
    {
        myMenu.actions().at( 5 )->setEnabled( false );  //Download
        myMenu.actions().at( 0 )->setEnabled( false );  //Reboot
    }

    // Show context menu at handling position
    myMenu.exec( globalPos );
}

void MainWindow::onSetHostSlot(QHostAddress host)
{
    ui->lineEditServerAddress->setText( host.toString() );
    setWindowTitle( "File browser " + host.toString() );
    ui->groupBoxServerAddress->hide();
}

void MainWindow::onAbortDeletionRequestedSlot()
{
    m_AbortDeleteRequested = true;
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

    QMessageBox msgBox( QMessageBox::Warning, "Delete", "Are you sure you want to delete this files", QMessageBox::Ok|QMessageBox::Cancel );
    if( msgBox.exec() )
    {
        qDebug() << "Deleting.......";
    }else
    {
        qDebug() << "Aborting reboot";
        return;
    }

    //Reset the abort deletion indicator
    m_AbortDeleteRequested = false;

    //Open the deletion dialog
    m_DialogDelete.show();

    //Get the list of files selected
    QStringList remotePathList;
    QString currentDir = ui->lineEditPath->text();
    if( m_ListViewEnabled )
    {
        QList<QTableWidgetItem *> selectedItems = ui->tableWidgetFileBrowser->selectedItems();

        //Go through each of the selected and add them to the list
        QListIterator<QTableWidgetItem*> iter( selectedItems );
        while( iter.hasNext() )
        {
            //Get the next item in the list
            QTableWidgetItem *item = iter.next();
            if( item == nullptr ) continue;
            if( item->column() > 0 ) continue;
            QString entryName = item->text();
            remotePathList.append( entryName );
        }
    }else
    {
        QList<QListWidgetItem *> selectedItems = ui->listWidgetFileBrowser->selectedItems();

        //Go through each of the selected and add them to the list
        QListIterator<QListWidgetItem*> iter( selectedItems );
        while( iter.hasNext() )
        {
            //Get the next item in the list
            QListWidgetItem *item = iter.next();
            if( item == nullptr ) continue;
            QString entryName = item->text();
            remotePathList.append( entryName );
        }
    }

    //Go through each of the selected
    QStringListIterator entryIter( remotePathList );
    while( entryIter.hasNext() )
    {
        //Process the event queue
        QApplication::processEvents();

        //Check if an abort was issued
        if( m_AbortDeleteRequested )
            return;

        //Wait between each delete as to not overwelm the amiga
        QThread::msleep( 100 );

        //Get the next item in the list
        QString entryName = entryIter.next();

        //Otherwise, display what we have
        QSharedPointer<DirectoryListing> directoryListing = m_DirectoryListings[ currentDir ];
        QSharedPointer<DirectoryListing> directoryEntry = directoryListing->findEntry( entryName );
        if( directoryEntry.isNull() )
            continue;

        //If this is a directory......
        if( directoryEntry->Type() == DET_USERDIR )
        {
            //Form the remote path
            QString remoteDirPath = currentDir;
            if( remoteDirPath.endsWith( ":" ) || remoteDirPath.endsWith( "/" ) )
                remoteDirPath += entryName;
            else
                remoteDirPath += "/" + entryName;

            //Update the deletion dialog
            emit currentFileBeingDeleted( remoteDirPath );
            QApplication::processEvents();

            //Clear out and delete this directory
            bool errorInSubdir = 0;
            deleteDirectoryRecursive( remoteDirPath, errorInSubdir );

            //Did we succeed in deleting the sub directory?
            if( errorInSubdir )
            {
                //Seems not.  Abort.  An error message for the user will have been presented in the deleteDirectoryRecursive() function
                return;
            }

            //Now delete this directory
            //emit deleteRemoteDirectorySignal( remoteDirPath );
            QString errorMessage;
            if( m_ProtocolHandler.deleteFile( remoteDirPath, errorMessage ) == false )
            {
                qDebug() << "Failed to delete path " << remoteDirPath;
                QMessageBox errorBox( QMessageBox::Critical, "Failed to delete remote directory", "An error occurred while deleting remote directory" + remoteDirPath, QMessageBox::Ok );
                errorBox.exec();
                return;
            }

            qDebug() << "Deleting file " << remoteDirPath;
        }

        //If it is a file to download
        if( directoryEntry->Type() == DET_FILE )
        {
            //Form the remote path
            QString nextRemoteFile = currentDir;
            if( nextRemoteFile.endsWith( ":" ) || nextRemoteFile.endsWith( "/" ) )
                nextRemoteFile += entryName;
            else
                nextRemoteFile += "/" + entryName;

            //Update the deletion dialog
            emit currentFileBeingDeleted( nextRemoteFile );
            QApplication::processEvents();

            //Send delete command for them
            //emit deleteRemoteDirectorySignal( remoteDirPath );
            QString errorMessage;
            if( m_ProtocolHandler.deleteFile( nextRemoteFile, errorMessage ) == false )
            {
                qDebug() << "Failed to delete path " << nextRemoteFile;
                QMessageBox errorBox( QMessageBox::Critical, "Failed to delete remote directory", "An error occurred while deleting remote directory" + nextRemoteFile, QMessageBox::Ok );
                errorBox.exec();
                return;
            }
            qDebug() << "Deleting file " << nextRemoteFile;
        }
    }

    //Hide the dialog
    emit deletionCompletedSignal();

    //Refresh the view
    emit getRemoteDirectorySignal( currentDir );

    //Hide the delete dialog
    m_DialogDelete.hide();
}

void MainWindow::onDownloadSelectedSlot()
{
    LOCK;

    static QString localDirPath = QDir::homePath();
    QList<QPair<QString, QString> > fileList;
    QStringList selectedFileEntries;

    //Get the list of files selected
    if( m_ListViewEnabled )
    {
        QList<QTableWidgetItem*> selectedItems = ui->tableWidgetFileBrowser->selectedItems();
        if( selectedItems.count() == 0 )    return;

        //Extract a list of entries
        QListIterator<QTableWidgetItem*> iter( selectedItems );
        while( iter.hasNext() )
        {
            QTableWidgetItem *item = iter.next();
            if( item->column() > 0 ) continue;
            QString entryName = item->text();
            selectedFileEntries.push_back( entryName );
            qDebug() << "Added entry " << entryName << " to the list.";
        }
    }else
    {
        QList<QListWidgetItem *> selectedItems = ui->listWidgetFileBrowser->selectedItems();
        if( selectedItems.count() == 0 )    return;     //Nothing to do then

        //extract a list of file/Dir names
        QListIterator<QListWidgetItem*> iter( selectedItems );
        while( iter.hasNext() )
        {
            QListWidgetItem *item = iter.next();
            QString entryName = item->text();
            selectedFileEntries.push_back( entryName );
        }
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

    QMessageBox msgBox( this );
    msgBox.setModal( false );
    msgBox.setWindowTitle( "Collecting Files" );
    msgBox.setText( "We are collecting the list of files from the server.\nThis may take some time.  Especially CDROMS or Floppies." );
    msgBox.setStandardButtons( QMessageBox::NoButton );
    msgBox.setIcon( QMessageBox::Information );
    msgBox.open();

    //Go through each of the selected
    QStringListIterator iter( selectedFileEntries );
    while( iter.hasNext() )
    {

        //Get the next item in the list
        QString entryName = iter.next();
        QString localFilePath = localDirPath + "/" + entryName;


        //Otherwise, display what we have
        QSharedPointer<DirectoryListing> directoryListing = m_DirectoryListings[ currentDir ];
        QSharedPointer<DirectoryListing> directoryEntry = directoryListing->findEntry( entryName );

        //If this is a directory that the user double clicked on
        //then we need opene that directory
        if( directoryEntry->Type() == DET_USERDIR )
        {
            //Form the remote path
            QString remoteDirPath = currentDir;
            if( remoteDirPath.endsWith( ":" ) || remoteDirPath.endsWith( "/" ) )
                remoteDirPath += entryName;
            else
                remoteDirPath += "/" + entryName;

            bool errorInSubdir = false;
            auto dirFiles = downloadPrepareLocalDirectory( localFilePath, remoteDirPath, errorInSubdir );
            if( errorInSubdir )
            {
                QMessageBox errorBox( QMessageBox::Critical, "Error creating local directory", "A local destination wasn't selected.", QMessageBox::Ok );
                errorBox.exec();
                msgBox.hide();
                return;
            }
            fileList.append( dirFiles );
        }

        //If it is a file to download
        if( directoryEntry->Type() == DET_FILE )
        {
            //Add the file to the list
            QString remoteFilePath = directoryEntry->Path();
            //qDebug() << "Download: Adding file " << localFilePath << " <------ " << remoteFilePath << " to the list";
            fileList.push_back( QPair<QString,QString>( localFilePath, remoteFilePath ) );
        }

        //If we have hiden info files, we should automatically add these to the list if they exist
        if( m_HideInfoFiles )
        {
            //Do we have an info file for this entry?
            QString infoFileName = entryName + ".info";
            QSharedPointer<DirectoryListing> infoEntry = directoryListing->findEntry( infoFileName );

            //Do we have one?
            if( !infoEntry.isNull() )
            {
                //It seems we do.  Let's add it to the list as well
                QString remoteInfoFilePath = infoEntry->Path();
                QString localInfoFilePath = localDirPath + "/" + infoEntry->Name();
                fileList.push_back( QPair<QString,QString>( localInfoFilePath, remoteInfoFilePath ) );
            }
        }
    }

    msgBox.hide();

    //Now that we have a list, trigger the download
    m_DialogDownloadFile.startDownload( fileList );
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
    m_DialogDownloadFile.connectToHost( serverAddress, port );

    //Connect the upload agent
    m_DialogUploadFile.connectToHost( serverAddress, port );

    //Enable the gui
    ui->listWidgetFileBrowser->setEnabled( true );
    ui->tableWidgetFileBrowser->setEnabled( true );
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
    m_DialogDownloadFile.disconnectFromhost();
    m_DialogUploadFile.disconnectFromhost();

    //Grey out the gui
    ui->listWidgetFileBrowser->setEnabled( false );
    ui->tableWidgetFileBrowser->setEnabled( false );
    ui->listWidgetDrives->setEnabled( false );
    ui->lineEditPath->setEnabled( false );
}

void MainWindow::onIncomingByteCountUpdateSlot( quint32 bytes )
{
    m_IncomingByteCount += bytes;
}

void MainWindow::onOutgoingByteCountUpdateSlot( quint32 bytes )
{
    m_OutgoingByteCount += bytes;
}

void MainWindow::onDeviceLeftSlot()
{
    onDisconnectButtonReleasedSlot();

    //Disconnect our upload and download dialogs
    m_DialogDownloadFile.disconnectFromhost();
    m_DialogUploadFile.disconnectFromhost();

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
    QString version( "Server Version: " + QString::number( major ) + "." + QString::number( minor ) + QString::number( revision ) );
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
}

void MainWindow::onVolumeListUpdateSlot( QStringList volumes )
{
    m_Volumes = volumes;
    updateDrivebrowser();
    if( ui->lineEditPath->text() == "" )
        updateFilebrowser();
}

void MainWindow::onAcknowledgeSlot( quint8 responseCode )
{
    switch( responseCode )
    {
        case 0:
            m_AcknowledgeState = AS_FAILED;
        break;
        case 1:
            m_AcknowledgeState = AS_SUCCESS;
        break;
        default:
            m_AcknowledgeState = AS_Unknown;
        break;
    }


}

void MainWindow::onThroughputTimerExpired()
{
    //Update the counters in the gui
    ui->labelUploadSpeed->setText( "Upload: " + QString::number( m_OutgoingByteCount/1024 ) + "kB/s" );
    ui->labelDownloadSpeed->setText( "Download: " + QString::number( m_IncomingByteCount/1024 ) + "kB/s" );

    //Clear the counters
    m_IncomingByteCount = 0;
    m_OutgoingByteCount = 0;
}

void MainWindow::onReconnectTimerExpired()
{
    this->onConnectButtonReleasedSlot();
}

void MainWindow::updateFilebrowser()
{
    LOCK;

    //First clear the existing view
    ui->listWidgetFileBrowser->clear();
    ui->tableWidgetFileBrowser->clear();
    while( ui->tableWidgetFileBrowser->rowCount() > 0 )
    {
           ui->tableWidgetFileBrowser->removeRow( 0 );
    }
    ui->tableWidgetFileBrowser->setHorizontalHeaderLabels( { "File", "Size" } );
    ui->tableWidgetFileBrowser->setColumnWidth( 0, 400 );


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
        QStringListIterator iter( m_Volumes );
        while( iter.hasNext() )
        {
            //Get the next volume
            QString volume = iter.next() + ":";
            QPixmap icon( ":/browser/icons/Harddisk_Amiga.png" );

            //Add this to the view
            if( m_ListViewEnabled )
            {
                //Create the row items
                QTableWidgetItem *iconItem = new QTableWidgetItem( icon, volume );
                QTableWidgetItem *fileSizeItem = new QTableWidgetItem( "-" );

                quint32 rowCount = ui->tableWidgetFileBrowser->rowCount();
                ui->tableWidgetFileBrowser->insertRow( rowCount );
                ui->tableWidgetFileBrowser->setItem( rowCount, 0, iconItem );
                ui->tableWidgetFileBrowser->setItem( rowCount, 1, fileSizeItem );
            }else
            {
                //Create the entry for our view
                QListWidgetItem *item = new QListWidgetItem( volume );

                item->setIcon( icon );
                item->setStatusTip( volume );

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

        if( m_ListViewEnabled )
        {
            //Create the row items
            QTableWidgetItem *iconItem = new QTableWidgetItem( *nextListEntry->Icon(), nextListEntry->Name() );
            QTableWidgetItem *fileSizeItem = new QTableWidgetItem( getSizeAsFormatedString( nextListEntry->Size() ) );

            quint32 rowCount = ui->tableWidgetFileBrowser->rowCount();
            ui->tableWidgetFileBrowser->insertRow( rowCount );
            ui->tableWidgetFileBrowser->setItem( rowCount, 0, iconItem );
            ui->tableWidgetFileBrowser->setItem( rowCount, 1, fileSizeItem );
        }else
        {
            //Create the entry for our view
            QString label = nextListEntry->Name() + "\n(" + QString::number( nextListEntry->Size()/1024 ) + "Kb)";
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
        QString fileSizeText = getSizeAsFormatedString( nextListEntry->Size() );
        QString fileNameLabel = nextListEntry->Name();
        QString toolTip = nextListEntry->Name() + "\n" + fileSizeText;

        if( m_ListViewEnabled )
        {
            //Create the row items
            QTableWidgetItem *iconItem = new QTableWidgetItem( *nextListEntry->Icon(), nextListEntry->Name() );
            QTableWidgetItem *fileSizeItem = new QTableWidgetItem( getSizeAsFormatedString( nextListEntry->Size() ) );

            quint32 rowCount = ui->tableWidgetFileBrowser->rowCount();
            ui->tableWidgetFileBrowser->insertRow( rowCount );
            ui->tableWidgetFileBrowser->setItem( rowCount, 0, iconItem );
            ui->tableWidgetFileBrowser->setItem( rowCount, 1, fileSizeItem );
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
    QStringListIterator iter( m_Volumes );
    while( iter.hasNext() )
    {
        //Get the next volume
        QString volume = iter.next() + ":";

        //Create the entry for our view
        QListWidgetItem *item = new QListWidgetItem( volume );
        QPixmap icon( ":/browser/icons/Harddisk_Amiga.png" );
        item->setIcon( icon );
        item->setStatusTip( volume );


        //Add this to the view
        ui->listWidgetDrives->addItem( item );
    }
}

void MainWindow::deleteDirectoryRecursive(QString remotePath, bool &error)
{
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
        QThread::msleep( 100 );

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
    m_AcknowledgeState = AS_Unknown;
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
    if( !timer.isActive() || m_AcknowledgeState != AS_SUCCESS )
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

