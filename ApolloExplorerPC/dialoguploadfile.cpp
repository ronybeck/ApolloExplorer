#include "dialoguploadfile.h"
#include "ui_dialoguploadfile.h"

#include <QMessageBox>
#include <QThread>
#include <QtEndian>

#include "VNetPCUtils.h"


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

        return;
    }

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

