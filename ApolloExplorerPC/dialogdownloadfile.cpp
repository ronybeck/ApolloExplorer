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
    m_DownloadActive( false )
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

    //qDebug() << "Connecting to server.";
    m_DownloadThread.onConnectToHostSlot( host, port );
    return;
}

void DialogDownloadFile::disconnectFromhost()
{
    LOCK;
    m_DownloadThread.onDisconnectFromHostRequestedSlot();
    m_DownloadCompletionWaitCondition.wakeAll();
}

void DialogDownloadFile::startDownload(QList<QSharedPointer<DirectoryListing> > remotePaths, QString localPath)
{
    m_DownloadActive = true;

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
    startDownload( files );
}

void DialogDownloadFile::startDownloadAndOpen(QList<QSharedPointer<DirectoryListing> > remotePaths)
{
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
    startDownload( remotePaths, tmpPath );
}

void DialogDownloadFile::startDownload( QList<QPair<QString, QString> > files )
{
    LOCK;

    //First check if there is already an operation underway
    if( !m_DownloadList.isEmpty() )
    {
        m_DownloadCompletionWaitCondition.notify_all();
        return;
    }

    //Activate the download
    m_DownloadActive = true;
    show();

    //Add this to our list
    m_DownloadList = files;

    //Is there anything to do here?
    if( m_DownloadList.isEmpty() )
    {
        resetDownloadDialog();
        emit singleFileDownloadCompletedSignal();
        m_DownloadCompletionWaitCondition.notify_all();
        return;
    }

    //Start the first file
    onSingleFileDownloadCompletedSlot();

}

bool DialogDownloadFile::isCurrentlyDownloading()
{
    return m_DownloadActive;
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
    //Clear out the download list
    m_DownloadList.clear();

    //Update the gui with a feedback
    resetDownloadDialog();
    hide();
    m_DownloadThread.onCancelDownloadSlot();
    m_DownloadCompletionWaitCondition.notify_all();

    emit allFilesDownloaded();
}


void DialogDownloadFile::onAbortedSlot( QString reason )
{
    LOCK;
    QMessageBox errorBox( "Download Aborted.", "The server aborted the download with: " + reason, QMessageBox::Critical, QMessageBox::Ok, 0, 0, this );
    errorBox.exec();
    resetDownloadDialog();
    hide();
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
        emit allFilesDownloaded();
        return;
    }

    //Get the next file from the list
    auto pair = m_DownloadList.front();
    m_DownloadList.pop_front();

    //signal to the download thread, which files to get next
    QString localFilePath = pair.first;
    QString remoteFilePath = pair.second;
    m_DownloadThread.onStartFileSlot( localFilePath, remoteFilePath );

    //Set the filename in the gui
    ui->labelDownload->setText( "File: " + remoteFilePath );
    ui->labelFilesRemaining->setText( "Files remaining: " + QString::number( m_DownloadList.count() ) );
}

void DialogDownloadFile::onAllFileDownloadsCompletedSlot()
{
    //Notify waiters
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

void DialogDownloadFile::resetDownloadDialog()
{
    if( !m_DownloadList.isEmpty() ) m_DownloadList.clear();
    m_DownloadActive = false;
}

QList<QPair<QString, QString> > DialogDownloadFile::prepareDownload(QString localPath, QString remotePath, bool &error)
{
    DBGLOG << "Preparing to downloading " << remotePath << " into " << localPath;
    QList<QPair<QString,QString>> files;

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

}

void DialogDownloadFile::onDisconnectedFromHostSlot()
{
    LOCK;
    m_DownloadCompletionWaitCondition.notify_all();
    if( m_DownloadList.count() )
    {
        QMessageBox errorBox( "Server Disconnected.", "The server disconnected with " + QString::number( m_DownloadList.count() ) + " files remaining.", QMessageBox::Critical, QMessageBox::Ok, 0, 0, this );
        errorBox.exec();
        resetDownloadDialog();
        emit singleFileDownloadCompletedSignal();
    }
}
