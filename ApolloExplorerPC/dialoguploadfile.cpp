#include "dialoguploadfile.h"
#include "ui_dialoguploadfile.h"

#include <QMessageBox>
#include <QThread>
#include <QtEndian>
#include <QFileInfo>
#include <QDir>
#include <QFile>

#define DEBUG 1
#include "AEUtils.h"


DialogUploadFile::DialogUploadFile(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogUploadFile),
    m_UploadThread( this ),
    m_UploadList( ),
    m_UploadRetryList( ),
    m_CurrentLocalFilePath( "" ),
    m_CurrentRemoteFilePath( "" )
{
    ui->setupUi(this);

    //Signals and Slots
    connect( this, &DialogUploadFile::startupUploadSignal, &m_UploadThread, &UploadThread::onStartFileSlot );
    connect( &m_UploadThread, &UploadThread::abortedSignal, this, &DialogUploadFile::onAbortedSlot );
    connect( &m_UploadThread, &UploadThread::connectedToServerSignal, this, &DialogUploadFile::onConnectedToHostSlot );
    connect( &m_UploadThread, &UploadThread::uploadCompletedSignal, this, &DialogUploadFile::onStartNextFileUploadSlot );
    connect( &m_UploadThread, &UploadThread::uploadProgressSignal, this, &DialogUploadFile::onProgressUpdate );
    connect( &m_UploadThread, &UploadThread::outgoingBytesSignal, this, &DialogUploadFile::outgoingBytesSignal );
    connect( &m_UploadThread, &UploadThread::uploadFailedSignal, this, &DialogUploadFile::onUploadFailedSlot );
    connect( &m_UploadThread, &UploadThread::directoryCreationCompletedSignal, this, &DialogUploadFile::onStartNextDirectoryCreationSlot );
    connect( &m_UploadThread, &UploadThread::directoryCreationFailedSignal, this, &DialogUploadFile::onDirectoryCreateFailedSlot );
    connect( &m_UploadThread, &UploadThread::operationTimedOutSignal, this, &DialogUploadFile::onOperationTimedOutSlot );
    connect( this, &DialogUploadFile::createDirectorySignal, &m_UploadThread, &UploadThread::onCreateDirectorySlot );

    //Gui signal slots
    connect( ui->buttonBoxCancel, &QDialogButtonBox::rejected, this, &DialogUploadFile::onCancelButtonReleasedSlot );


    //Set the window title
    this->setWindowTitle( "Upload" );

    //Start the upload thread
    m_UploadThread.start();
}

DialogUploadFile::~DialogUploadFile()
{
    m_UploadThread.stopThread();
    m_UploadThread.wait();
    delete ui;
}

void DialogUploadFile::connectToHost(QHostAddress host, quint16 port)
{
    //qDebug() << "Connecting to server.";
    m_Host=host;
    m_Port=port;
    m_UploadThread.onConnectToHostSlot( host, port );
}

void DialogUploadFile::disconnectFromhost()
{
    //qDebug() << "Disconnecting to server.";
    m_UploadThread.onDisconnectFromHostRequestedSlot();
    cleanup();
    resetDialog();
}

void DialogUploadFile::startUpload( QSharedPointer<DirectoryListing> remotePath, QStringList localPaths)
{
    if( m_UploadList.size() > 0 )
        return; //Sorry, but we are already uploading

    //Set the window title and retry count
    m_RetryCount = RETRY_COUNT;
    this->setWindowTitle( "Uploading...." );
    ui->progressBar->setValue( 0 );
    ui->labelFilesRemaining->setText( "Files remaing: - ");
    ui->labelSpeed->setText( "Speed: -" );
    ui->labelUpload->setText( "Initialising upload." );

    show();
    //we need to parse list of local paths and check for directories
    QStringListIterator localPathIter( localPaths );
    QList<QPair<QString,QString>> uploadList;
    bool error = false;

    //Go through the local list of files
    while( localPathIter.hasNext() )
    {
        QString localFilePath = localPathIter.next();
        QString remoteFilePath = remotePath->Path();

        //Get the file information for this path
        QFileInfo localFileInfo( localFilePath );

        //get the Filename
        QString localFilename = localFileInfo.fileName();
        if( localFileInfo.isDir() )
        {
            //Due to a bug in Qt5 on MacOS, I can't rely on QFileInfo::filename() to get the name of the directory (without the path)
            const QDir localDirectory(localFilePath);
            localFilename = localDirectory.dirName();
        }

        //We need to add the subdirectory/filename to the remote path
        if( remoteFilePath.endsWith( ":" ) )
            remoteFilePath += localFilename;
        else
            remoteFilePath += "/" + localFilename;

        //If this is a directory, we need to create it
        if( localFileInfo.isDir() )
        {
            auto subDirList = createRemoteDirectoryStructure( localFilePath, remoteFilePath, error );
            if( error )
            {
                WARNLOG << "Failed to create the directory structure " << remoteFilePath;
                return;
            }

            //Append this to the list
            uploadList.append( subDirList );
            continue;
        }

        //If this is a file, just add it to the list
        uploadList.append( QPair<QString,QString>( localFilePath, remoteFilePath ) );
    }

    //Start the next upload
    m_UploadList.append( uploadList );
    onStartNextDirectoryCreationSlot();
}

QList<QPair<QString,QString>> DialogUploadFile::createRemoteDirectoryStructure(QString localPath, QString remotePath, bool& error )
{
    QList<QPair<QString,QString>> files;
    DBGLOG << "Creating directory " << remotePath;
    ui->labelUpload->setText( "Creating directory " + remotePath );

    //Add this directory to the list of directories to create
    m_RemoteDirectories.append( remotePath );

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
            DBGLOG << "Adding file " << nextLocalPath << " ------> " << nextRemotePath << " to upload list.";
            QPair<QString, QString> filePair( nextLocalPath, nextRemotePath );
            files.append( filePair );
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
            auto subdirFiles = createRemoteDirectoryStructure( nextLocalPath, nextRemotePath, errorOnSubdir );
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

void DialogUploadFile::resetDialog()
{
    //Reset the gui elements
    ui->labelFilesRemaining->setText( "Remaining: - " );
    ui->labelSpeed->setText( "speed: - " );
    ui->labelUpload->setText( "Uploading: - " );
    ui->progressBar->setValue( 0 );
}

void DialogUploadFile::onCancelButtonReleasedSlot()
{
    //Signal a cancel to the upload thread
    resetDialog();
    cleanup();
    hide();
}

void DialogUploadFile::onConnectedToHostSlot()
{
    DBGLOG << "Upload thread Connected.";

    connect( &m_UploadThread, &UploadThread::disconnectedFromServerSignal, this, &DialogUploadFile::onDisconnectedFromHostSlot );

    //Do we have tasks to continue?
    if( m_UploadList.count() )
    {
        DBGLOG << "Restarting upload.";
        ui->labelUpload->setText( "Restarting upload." );
        onStartNextFileUploadSlot();
    }
}

void DialogUploadFile::onDisconnectedFromHostSlot()
{
    DBGLOG << "Upload thread disconnected.";
    connect( &m_UploadThread, &UploadThread::disconnectedFromServerSignal, this, &DialogUploadFile::onDisconnectedFromHostSlot );
}

void DialogUploadFile::onStartNextFileUploadSlot()
{
    //Do we have any files to upload?
    if( m_UploadList.count() == 0 )
    {
        //Should we do the retry list now?
        if( m_RetryCount>0 && m_UploadRetryList.size() > 0 )
        {
            this->setWindowTitle( "Re-sending failed files." );
            m_UploadList.append( m_UploadRetryList );
            m_UploadRetryList.clear();
            m_RetryCount--;
        }else
        {
            //We are done
            resetDialog();
            hide();
            emit allFilesUploadedSignal();
            return;
        }
    }

    //Show the GUI
    if( isHidden() )    show();

    //Take the first file at the head of the list
    QPair<QString,QString> filefileUpload = m_UploadList.front();
    m_UploadList.pop_front();
    m_CurrentLocalFilePath = filefileUpload.first;
    m_CurrentRemoteFilePath = filefileUpload.second;

    //Update gui
    ui->progressBar->setValue( 0 );
    ui->labelUpload->setText( "File: " + m_CurrentLocalFilePath );
    ui->labelFilesRemaining->setText( "Files: " + QString::number( m_UploadList.count() ) );

    //Start this upload
    DBGLOG << "Uploading " << m_CurrentLocalFilePath << " to  " << m_CurrentRemoteFilePath;
    emit startupUploadSignal( m_CurrentLocalFilePath, m_CurrentRemoteFilePath );
}

void DialogUploadFile::onStartNextDirectoryCreationSlot()
{
    //Check if we have more directories to create
    if( m_RemoteDirectories.size() == 0 )
    {
        //Let's kick off the upload of files now
        onStartNextFileUploadSlot();
        return;
    }

    //Update the gui
    ui->labelUpload->setText( "Creating Directory: " + m_CurrentRemoteFilePath );

    //Get the next directory to create
    m_CurrentRemoteFilePath = m_RemoteDirectories.front();
    m_RemoteDirectories.pop_front();
    emit createDirectorySignal( m_CurrentRemoteFilePath );
}

void DialogUploadFile::onDirectoryCreateFailedSlot()
{
    QMessageBox errorBox( "Error Creating directory", "We failed to create the directory " + m_CurrentRemoteFilePath + ".  Upload aborted.", QMessageBox::Critical, QMessageBox::Ok, 0, 0, this );
    errorBox.show();
    resetDialog();
    cleanup();
    hide();
}

void DialogUploadFile::onUploadFailedSlot( UploadThread::UploadFailureType type )
{
    Q_UNUSED( type )
    //Now we should reschedule the upload
    m_UploadRetryList.append( QPair<QString,QString>( m_CurrentLocalFilePath, m_CurrentRemoteFilePath ) );
    DBGLOG << "Adding file " << m_CurrentLocalFilePath << " to the retry list.";
    onStartNextFileUploadSlot();
}

void DialogUploadFile::onAbortedSlot( QString reason )
{
    Q_UNUSED( reason );

    m_UploadRetryList.append( QPair<QString,QString>(m_CurrentLocalFilePath,m_CurrentRemoteFilePath) );
    onStartNextFileUploadSlot();
}

void DialogUploadFile::onProgressUpdate(quint8 procent, quint64 bytes, quint64 throughput)
{
    Q_UNUSED( bytes )
    ui->labelSpeed->setText( "speed: " + QString::number( throughput/1024 ) + "KB/s" );
    ui->progressBar->setValue( procent );
}

void DialogUploadFile::onOperationTimedOutSlot()
{
    DBGLOG << "Operation timedout";
    //If we are uploading, we should reconnect and then reschedule the current upload
    DBGLOG << "Reconnecting";
    ui->labelUpload->setText( "Operation timed out.  Retrying....." );
    disconnect( &m_UploadThread, &UploadThread::disconnectedFromServerSignal, this, &DialogUploadFile::onDisconnectedFromHostSlot );
    m_UploadThread.onDisconnectFromHostRequestedSlot();
    m_UploadThread.onConnectToHostSlot( m_Host, m_Port );

    //Now we should reschedule the upload
    m_UploadRetryList.append( QPair<QString,QString>( m_CurrentLocalFilePath, m_CurrentRemoteFilePath ) );
}

void DialogUploadFile::cleanup()
{
    m_RemoteDirectories.clear();
    m_UploadList.clear();
    m_UploadRetryList.clear();
    m_CurrentLocalFilePath.clear();
    m_CurrentRemoteFilePath.clear();
}

