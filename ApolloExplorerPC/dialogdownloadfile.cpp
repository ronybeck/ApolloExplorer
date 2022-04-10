#include "dialogdownloadfile.h"
#include "ui_dialogdownloadfile.h"

#include <QMessageBox>
#include <QDebug>

#include "VNetPCUtils.h"

#define LOCK QMutexLocker locker( &m_Mutex )
#define UNLOCK locker.unlock()
#define RELOCK locker.relock()

DialogDownloadFile::DialogDownloadFile(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogDownloadFile),
    m_DownloadThread(),
    m_DownloadList( ),
    m_Mutex( QMutex::Recursive )
{
    ui->setupUi(this);

    //Signals and Slots
    connect( &m_DownloadThread, &DownloadThread::connectToHostSignal, this, &DialogDownloadFile::onConnectedToHostSlot );
    connect( this, &DialogDownloadFile::startupDownloadSignal, &m_DownloadThread, &DownloadThread::onStartFileSlot );
    connect( &m_DownloadThread, &DownloadThread::abortedSignal, this, &DialogDownloadFile::onAbortedSlot );
    connect( &m_DownloadThread, &DownloadThread::connectedToServerSignal, this, &DialogDownloadFile::onConnectedToHostSlot );
    connect( &m_DownloadThread, &DownloadThread::disconnectedFromServerSignal, this, &DialogDownloadFile::onDisconnectedFromHostSlot );
    connect( &m_DownloadThread, &DownloadThread::downloadCompletedSignal, this, &DialogDownloadFile::onDownloadCompletedSlot );
    connect( &m_DownloadThread, &DownloadThread::downloadProgressSignal, this, &DialogDownloadFile::onProgressUpdate );

    //Gui signal slots
    connect( ui->buttonBox, &QDialogButtonBox::rejected, this, &DialogDownloadFile::onCancelButtonReleasedSlot );

    //Set the windows title
    this->setWindowTitle( "Download" );

    //Start the download thread
    m_DownloadThread.start();
}

DialogDownloadFile::~DialogDownloadFile()
{
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
}

void DialogDownloadFile::startDownload( QList<QPair<QString, QString> > files )
{
    LOCK;

    //First check if there is already an operation underway
    if( !m_DownloadList.isEmpty() )
        return;

    show();
    //Clear out the old list
    if( !m_DownloadList.isEmpty() )
        m_DownloadList.clear();

    //Add this to our list
    m_DownloadList = files;

    //Is there anything to do here?
    if( m_DownloadList.isEmpty() )
        return;

    //Start the first file
    onDownloadCompletedSlot();

}

bool DialogDownloadFile::isCurrentlyDownloading()
{
    LOCK;
    if( m_DownloadList.count( ) )
        return true;

    return false;
}

void DialogDownloadFile::onCancelButtonReleasedSlot()
{
    LOCK;
    //Clear out the download list
    m_DownloadList.clear();

    //Update the gui with a feedback
    resetDownloadDialog();

    m_DownloadThread.onCancelDownloadSlot();
}


void DialogDownloadFile::onAbortedSlot( QString reason )
{
    LOCK;
    QMessageBox errorBox( "Download Aborted.", "The server aborted the download with: " + reason, QMessageBox::Critical, QMessageBox::Ok, 0, 0, this );
    errorBox.exec();
    resetDownloadDialog();
}

void DialogDownloadFile::onDownloadCompletedSlot()
{
    LOCK;
    //Do we have any more files to donload?
    if( m_DownloadList.isEmpty() )
    {
        //Looks like we are done
        resetDownloadDialog();
        hide();
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

void DialogDownloadFile::onProgressUpdate(quint8 procent, quint64 bytes, quint64 throughput)
{
    LOCK;

    //Update the gui
    ui->progressBar->setValue( procent );
    ui->labelSpeed->setText( "Speed: " + QString::number( throughput / 1024 ) + "kb/s" );
}

void DialogDownloadFile::resetDownloadDialog()
{
    if( !m_DownloadList.isEmpty() ) m_DownloadList.clear();
}

void DialogDownloadFile::onConnectedToHostSlot()
{

}

void DialogDownloadFile::onDisconnectedFromHostSlot()
{
    LOCK;
    if( m_DownloadList.count() )
    {
        QMessageBox errorBox( "Server Disconnected.", "The server disconnected with " + QString::number( m_DownloadList.count() ) + " files remaining.", QMessageBox::Critical, QMessageBox::Ok, 0, 0, this );
        errorBox.exec();
        resetDownloadDialog();
    }
}
