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
    m_CurrentLocalFilePath( "" ),
    m_CurrentRemoteFilePath( "" )
{
    ui->setupUi(this);

    //Signals and Slots
    connect( this, &DialogUploadFile::startupUploadSignal, &m_UploadThread, &UploadThread::onStartFileSlot );
    connect( &m_UploadThread, &UploadThread::abortedSignal, this, &DialogUploadFile::onAbortedSlot );
    connect( &m_UploadThread, &UploadThread::connectedToServerSignal, this, &DialogUploadFile::onConnectedToHostSlot );
    connect( &m_UploadThread, &UploadThread::disconnectedFromServerSignal, this, &DialogUploadFile::onDisconnectedFromHostSlot );
    connect( &m_UploadThread, &UploadThread::uploadCompletedSignal, this, &DialogUploadFile::onUploadCompletedSlot );
    connect( &m_UploadThread, &UploadThread::uploadProgressSignal, this, &DialogUploadFile::onProgressUpdate );

    //Gui signal slots
    connect( ui->buttonBoxCancel, &QDialogButtonBox::rejected, this, &DialogUploadFile::onCancelButtonReleasedSlot );


    //Set the window title
    this->setWindowTitle( "Upload" );

    //Start the upload thread
    m_UploadThread.start();
}

DialogUploadFile::~DialogUploadFile()
{
    delete ui;
}

void DialogUploadFile::connectToHost(QHostAddress host, quint16 port)
{
    //qDebug() << "Connecting to server.";
    m_UploadThread.onConnectToHostSlot( host, port );
}

void DialogUploadFile::disconnectFromhost()
{
    //qDebug() << "Disconnecting to server.";
    m_UploadThread.onDisconnectFromHostRequestedSlot();
}

void DialogUploadFile::startUpload(QList<QPair<QString, QString> > files)
{
    //Clear out the old list
    if( !m_UploadList.isEmpty() )
        return; //No new uploads while this one is in progress

    //Set the new list
    m_UploadList.append( files );

    //Start the next upload
    onUploadCompletedSlot();
}

void DialogUploadFile::startUpload( QSharedPointer<DirectoryListing> remotePath, QStringList localPaths)
{
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

        //We need to add the subdirectory/filename to the remote path
        if( remoteFilePath.endsWith( ":" ) )
            remoteFilePath += localFileInfo.fileName();
        else
            remoteFilePath += "/" + localFileInfo.fileName();

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
    onUploadCompletedSlot();
}

QList<QPair<QString,QString>> DialogUploadFile::createRemoteDirectoryStructure(QString localPath, QString remotePath, bool& error )
{
    QList<QPair<QString,QString>> files;
    DBGLOG << "Creating directory " << remotePath;
    ui->labelUpload->setText( "Creating directory " + remotePath );

    //First, lets create this path
    if( !m_UploadThread.createDirectory( remotePath ) )
    {
        error = true;
        DBGLOG << "Failed to create remote directory " << remotePath;
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
            DBGLOG << "Adding file " << nextLocalPath << " ------> " << nextRemotePath << " to upload list.";
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

void DialogUploadFile::onCancelButtonReleasedSlot()
{
    //Signal a cancel to the upload thread
    m_UploadThread.onCancelUploadSlot();

    //Clear out out file list
    m_UploadList.clear();

    hide();
}

void DialogUploadFile::onConnectedToHostSlot()
{
    qDebug() << "Upload thread Connected.";
}

void DialogUploadFile::onDisconnectedFromHostSlot()
{
    qDebug() << "Upload thread disconnected.";
}

void DialogUploadFile::onUploadCompletedSlot()
{
    //Do we have any files to upload?
    if( m_UploadList.count() == 0 )
    {
        //We are done
        hide();
        emit allFilesUploadedSignal();
        return;
    }

    //Show the GUI
    if( isHidden() )    show();

    //Take the first file at the head of the list
    QPair<QString,QString> filefileUpload = m_UploadList.front();
    m_UploadList.pop_front();
    m_CurrentLocalFilePath = filefileUpload.first;
    m_CurrentRemoteFilePath = filefileUpload.second;

    //Update gui
    ui->labelUpload->setText( "File: " + m_CurrentLocalFilePath );
    ui->labelFilesRemaining->setText( "Files: " + QString::number( m_UploadList.count() ) );

    //Start this upload
    qDebug() << "Uploading " << m_CurrentLocalFilePath << " to  " << m_CurrentRemoteFilePath;
    emit startupUploadSignal( m_CurrentLocalFilePath, m_CurrentRemoteFilePath );
}

void DialogUploadFile::onAbortedSlot( QString reason )
{
    //Clear out our list
    m_UploadList.clear();

    QMessageBox errorBox( "Error uploading file", "The local file couldn't be uploaded because: " + reason, QMessageBox::Critical, QMessageBox::Ok, 0, 0, this );
    errorBox.exec();
}

void DialogUploadFile::onProgressUpdate(quint8 procent, quint64 bytes, quint64 throughput)
{
    ui->labelSpeed->setText( "speed: " + QString::number( throughput/1024 ) + "kb/s" );
    ui->progressBar->setValue( procent );
}

