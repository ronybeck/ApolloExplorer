#include "dialogdownloadfile.h"
#include "ui_dialogdownloadfile.h"

#include <QMessageBox>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QDesktopServices>

#include "AEUtils.h"

#define LOCK QMutexLocker locker( &m_Mutex )
#define UNLOCK locker.unlock()
#define RELOCK locker.relock()

DialogDownloadFile::DialogDownloadFile(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogDownloadFile),
    m_DownloadThread(),
    m_DownloadList( ),
    m_Mutex( QMutex::Recursive ),
    m_FilesToOpen( ),
    m_OperationState( IDLE ),
    m_ConnectionState( DISCONNECTED )
{
    ui->setupUi(this);

    //Signals and Slots
    connect( &m_DownloadThread, &DownloadThread::connectToHostSignal, this, &DialogDownloadFile::onConnectedToHostSlot );
    connect( this, &DialogDownloadFile::startDownloadSignal, &m_DownloadThread, &DownloadThread::onStartFileSlot );
    connect( &m_DownloadThread, &DownloadThread::abortedSignal, this, &DialogDownloadFile::onAbortedSlot );
    connect( &m_DownloadThread, &DownloadThread::connectedToServerSignal, this, &DialogDownloadFile::onConnectedToHostSlot );
    connect( &m_DownloadThread, &DownloadThread::disconnectedFromServerSignal, this, &DialogDownloadFile::onDisconnectedFromHostSlot );
    connect( &m_DownloadThread, &DownloadThread::downloadCompletedSignal, this, &DialogDownloadFile::onSingleFileDownloadCompletedSlot );
    connect( &m_DownloadThread, &DownloadThread::downloadProgressSignal, this, &DialogDownloadFile::onProgressUpdate );
    connect( this, &DialogDownloadFile::allFilesDownloaded, this, &DialogDownloadFile::onAllFileDownloadsCompletedSlot );
    connect( &m_DownloadThread, &DownloadThread::incomingBytesSignal, this, &DialogDownloadFile::incomingBytesSignal );
    connect( &m_DownloadThread, &DownloadThread::operationTimedOutSignal, this, &DialogDownloadFile::onOperationTimedoutSlot );

    //Gui signal slots
    connect( ui->buttonBox, &QDialogButtonBox::rejected, this, &DialogDownloadFile::onCancelButtonReleasedSlot );

    //Set the windows title
    this->setWindowTitle( "Download" );

    //Start the download thread
    m_DownloadThread.start();
}

DialogDownloadFile::~DialogDownloadFile()
{
    m_DownloadThread.stopThread();
    m_DownloadThread.wait();
    delete ui;
}

void DialogDownloadFile::connectToHost(QHostAddress host, quint16 port)
{
    LOCK;
    if( m_ConnectionState != DISCONNECTED )
        return;

    DBGLOG << "Connecting to host";
    m_ConnectionState=CONNECTING;
    m_Host=host;
    m_Port=port;
    m_DownloadThread.onConnectToHostSlot( host, port );
    return;
}

void DialogDownloadFile::disconnectFromhost()
{
    LOCK;

    if( m_ConnectionState == DISCONNECTED )
        return;

    DBGLOG << "Disconnecting from host";
    m_ConnectionState = DISCONNECTING;
    m_OperationState = IDLE;
    m_DownloadThread.onDisconnectFromHostRequestedSlot();
    m_DownloadCompletionWaitCondition.wakeAll();
    resetDownloadDialog();
}

void DialogDownloadFile::startDownload(QList<QSharedPointer<DirectoryListing> > remotePaths, QString localPath)
{

    //Show the gui
    show();

    //First check if there is already an operation underway
    if( m_OperationState != IDLE  )
        return;

    //Set the operation state
    m_OperationState = GETTING_DIRECTORY;

    LOCK;
    DBGLOG << "Starting download.";
    QList<QPair<QString,QString>> files;

    //Show the gui
    show();
    ui->progressBar->setValue( 0 );

    //Build up the list of files to download
    QListIterator<QSharedPointer<DirectoryListing>> iter( remotePaths );
    while( iter.hasNext() )
    {
        auto entry = iter.next();

        //If the entry is a file, just add it to our list
        if( entry->Type() == DET_FILE )
        {
            QString localFilePath = localPath + "/" + entry->Name();
            QString remoteFilePath = entry->Path();
            files.append( QPair<QString,QString>( localFilePath, remoteFilePath ) );
        }

        //If the entry is a directory, we need to recurse into the directory
        if( entry->Type() == DET_USERDIR )
        {
            bool error = false;
            QString subdirPath = localPath + "/" + entry->Name();
            QList<QPair<QString,QString>> subDirFiles = prepareDownload( subdirPath, entry->Path(), error );
            files.append( subDirFiles );

            //What happens if there is an error?
            if( error )
            {
                //Well, we give up
                hide();
                QMessageBox errorBox( "Getting remote Subdirectories failed..", "For reasons unknown, we couldn't get all remote directories.", QMessageBox::Critical, QMessageBox::Ok, 0, 0, this );
                errorBox.exec();
                resetDownloadDialog();
                emit singleFileDownloadCompletedSignal();
            }
        }
    }

    //Now start the download
    m_OperationState=IDLE;
    startDownload( files );
}

void DialogDownloadFile::startDownloadAndOpen(QList<QSharedPointer<DirectoryListing> > remotePaths)
{
    LOCK;

    //Show the gui
    show();

    //First check if there is already an operation underway
    if( m_OperationState != IDLE  )
        return;

    //Set the operation state
    m_OperationState = DOWNLOADING_FILES;

    //First create the temp directory
    QString tmpPath = QDir::tempPath() + "/ApolloExplorer/";
    QDir tmpDir( tmpPath );
    if( !tmpDir.exists() )
    {
        DBGLOG << "Temp path " << tmpPath << " doesn't exist.  Creating it.";
        tmpDir.mkpath( tmpPath );
    }
    //We want to build a list of files that will be downloaded
    m_FilesToOpen.clear();
    QListIterator<QSharedPointer<DirectoryListing>> iter( remotePaths );
    while( iter.hasNext() )
    {
        auto nextEntry = iter.next();
        if( nextEntry->Type() == DET_FILE )
        {
            QString destinationPath = tmpPath + nextEntry->Name();
            m_FilesToOpen.push_back( destinationPath );
        }
    }

    //Now start the download
    m_OperationState=IDLE;
    startDownload( remotePaths, tmpPath );
}

void DialogDownloadFile::startDownload( QList<QPair<QString, QString> > files )
{
    LOCK;

    //Show the gui
    show();

    //First check if there is already an operation underway
    if( m_OperationState != IDLE )
        return;

    //Activate the download
    m_OperationState=DOWNLOADING_FILES;

    //Add this to our list
    m_DownloadList = files;

    //Is there anything to do here?
    if( m_DownloadList.isEmpty() )
    {
        resetDownloadDialog();
        emit singleFileDownloadCompletedSignal();
        m_DownloadCompletionWaitCondition.notify_all();
        m_OperationState=IDLE;
        return;
    }

    //Start the first file
    onSingleFileDownloadCompletedSlot();
}

bool DialogDownloadFile::isCurrentlyDownloading()
{
    return m_OperationState != IDLE;
}

void DialogDownloadFile::waitForDownloadToComplete()
{
    m_DownloadCompletionMutex.lock();
    m_DownloadCompletionWaitCondition.wait( &m_DownloadCompletionMutex );
    m_DownloadCompletionMutex.unlock();
}

void DialogDownloadFile::onCancelButtonReleasedSlot()
{
    LOCK;
    DBGLOG << "Cancelling download";
    //Clear out the download list
    m_DownloadList.clear();

    //Update the gui with a feedback
    resetDownloadDialog();
    hide();
    m_OperationState=IDLE;
    m_DownloadThread.onCancelDownloadSlot();
    m_DownloadCompletionWaitCondition.notify_all();

    emit allFilesDownloaded();
}


void DialogDownloadFile::onAbortedSlot( QString reason )
{
    LOCK;
    DBGLOG << "Download aborted";
    QMessageBox errorBox( "Download Aborted.", "The server aborted the download with: " + reason, QMessageBox::Critical, QMessageBox::Ok, 0, 0, this );
    errorBox.exec();
    resetDownloadDialog();
    hide();
    m_OperationState=IDLE;
    m_DownloadCompletionWaitCondition.notify_all();
    emit allFilesDownloaded();
}

void DialogDownloadFile::onSingleFileDownloadCompletedSlot()
{
    LOCK;
    //Do we have any more files to donload?
    if( m_DownloadList.isEmpty() )
    {
        //Looks like we are done
        resetDownloadDialog();
        hide();
        m_OperationState=IDLE;
        emit allFilesDownloaded();
        return;
    }

    //Get the next file from the list
    auto pair = m_DownloadList.front();
    m_DownloadList.pop_front();

    //signal to the download thread, which files to get next
    m_CurrentLocalFile = pair.first;
    m_CurrentRemoteFile = pair.second;
    m_DownloadThread.onStartFileSlot( m_CurrentLocalFile, m_CurrentRemoteFile );

    //Set the filename in the gui
    ui->labelDownload->setText( "File: " + m_CurrentRemoteFile );
    ui->labelFilesRemaining->setText( "Files remaining: " + QString::number( m_DownloadList.count() ) );
}

void DialogDownloadFile::onAllFileDownloadsCompletedSlot()
{
    //Notify waiters
    m_OperationState=IDLE;
    m_DownloadCompletionWaitCondition.notify_all();

    //Now check if we have some files to open
    if( m_FilesToOpen.size() == 0)  return;

    //Go through each file and open it
    QStringListIterator iter( m_FilesToOpen );
    while( iter.hasNext() )
    {
        QString filePath = iter.next();
        QUrl file = QUrl::fromLocalFile( filePath );
        DBGLOG << "Opening file " << file.toString();
        QDesktopServices::openUrl( file );
    }

    //Cleanup
    m_FilesToOpen.clear();
}

void DialogDownloadFile::onProgressUpdate(quint8 procent, quint64 bytes, quint64 throughput)
{
    Q_UNUSED( bytes )
    LOCK;

    //Update the gui
    ui->progressBar->setValue( procent );
    ui->labelSpeed->setText( "Speed: " + QString::number( throughput / 1024 ) + "KB/s" );
}

void DialogDownloadFile::onOperationTimedoutSlot()
{
    DBGLOG << "Operation Timedout.";
    //Indicate to the user that the timeout has happened
    ui->labelDownload->setText( "Operation timed out.   Restarting download." );
    ui->progressBar->setValue( 0 );

    //If we were downloading, requeue the current file
    if( m_OperationState == DOWNLOADING_FILES && m_CurrentLocalFile.length() > 0 )
    {
        DBGLOG << "Requing file " << m_CurrentRemoteFile << " for download";
        QPair<QString, QString> file( m_CurrentLocalFile, m_CurrentRemoteFile );
        m_CurrentRemoteFile = "";
        m_CurrentLocalFile = "";
        m_DownloadList.push_front( file );
    }

    //We should disconnect, reconnect and restart the download process
    DBGLOG << "Disconencting";
    m_DownloadThread.onDisconnectFromHostRequestedSlot();
    DBGLOG << "Reconnecting";
    m_DownloadThread.onConnectToHostSlot( m_Host, m_Port );
}

void DialogDownloadFile::resetDownloadDialog()
{
    m_DownloadList.clear();
    ui->labelDownload->setText( "File: -" );
    ui->labelFilesRemaining->setText( "Files remaining: -" );
    ui->progressBar->setValue( 0 );
}

QList<QPair<QString, QString> > DialogDownloadFile::prepareDownload(QString localPath, QString remotePath, bool &error)
{
    DBGLOG << "Preparing to downloading " << remotePath << " into " << localPath;
    QList<QPair<QString,QString>> files;

    //Set the operation state
    m_OperationState=GETTING_DIRECTORY;

    //Get the directory listing for the remote path
    QSharedPointer<DirectoryListing> remoteDirectoryListing = m_DownloadThread.onGetDirectoryListingSlot( remotePath );
    if( remoteDirectoryListing.isNull() )
    {
        DBGLOG << "Unable to get remote path " << remotePath;
        QMessageBox errorBox( QMessageBox::Critical, "Failed to get remote directory", "An error occurred while retrieving remote directory" + remotePath, QMessageBox::Ok );
        errorBox.exec();
        error = true;
        return files;
    }

    //First off, make sure the local directory exists
    QFileInfo localDirInfo( localPath );
    if( !localDirInfo.exists() )
    {
        QDir localDir;
        if( ! localDir.mkdir( localPath ) )
        {
            qDebug() << "Unable to create " << localPath << " for remote path " << remotePath;
            QMessageBox errorBox( QMessageBox::Critical, "Error creating local directory", "Unable to create directory " + localPath, QMessageBox::Ok );
            errorBox.exec();
            error = true;
            return files;
        }
    }

    //Now that we have a new listing (hopefully), we can go through the list of files we need to get
    QVectorIterator<QSharedPointer<DirectoryListing>> iter( remoteDirectoryListing->Entries() );
    while( iter.hasNext() )
    {
        //Get the next entry
        QSharedPointer<DirectoryListing> listing = iter.next();

        //Update the gui
        ui->labelDownload->setText("Searching remote path: " + listing->Path() );

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
            auto subdirFiles = prepareDownload( nextLocalPath, nextRemotePath, errorInSubdir );
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

void DialogDownloadFile::onConnectedToHostSlot()
{
    m_ConnectionState=CONNECTED;

    //If we were in the middle of a download previously, then we should restart the download
    if( m_OperationState == DOWNLOADING_FILES )
    {
        DBGLOG << "Restarting download";
        ui->labelDownload->setText( "Restarting download." );
        onSingleFileDownloadCompletedSlot();
    }
}

void DialogDownloadFile::onDisconnectedFromHostSlot()
{
    LOCK;
    DBGLOG << "Disconnected";
    m_ConnectionState=DISCONNECTED;
    m_DownloadCompletionWaitCondition.notify_all();

    //If we were in the middle of a download, restart
    if( m_OperationState != IDLE )
        onOperationTimedoutSlot();
    else
        m_DownloadThread.onConnectToHostSlot( m_Host, m_Port );
}
