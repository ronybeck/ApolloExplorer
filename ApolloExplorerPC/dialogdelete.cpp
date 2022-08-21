#include "dialogdelete.h"
#include "ui_dialogdelete.h"

#include <QMessageBox>


DialogDelete::DialogDelete(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogDelete),
    m_DeletionThread(),
    m_RemotePaths(),
    m_CurrentListing(),
    m_ActiveRecursiveDeletion( false )
{
    ui->setupUi(this);
    connect( ui->pushButton, &QPushButton::released, this, &DialogDelete::onCancelButtonReleasedSlot );
    connect( &m_DeletionThread, &DeletionThread::fileDeletedSignal, this, &DialogDelete::onCurrentFileBeingDeletedSlot );
    connect( &m_DeletionThread, &DeletionThread::recursiveDeletionCompletedSignal, this, &DialogDelete::onRecursiveDeleteCompletedSlot );

    m_DeletionThread.start();
}

DialogDelete::~DialogDelete()
{
    m_DeletionThread.stopThread();
    m_DeletionThread.wait();
    delete ui;
}

void DialogDelete::connectToHost(QHostAddress host, quint16 port)
{
    m_DeletionThread.onConnectToHostSlot( host, port );
}

void DialogDelete::disconnectFromhost()
{
    m_DeletionThread.onDisconnectFromHostRequestedSlot();
}

void DialogDelete::onDeleteRemotePathsSlot(QList<QSharedPointer<DirectoryListing> > remotePaths)
{
    //Start the deletion process
    m_RemotePaths = remotePaths;
    doNextDeletion();
}

void DialogDelete::onCurrentFileBeingDeletedSlot( QString filepath )
{
    ui->label->setText( "Deleting: " + filepath );
    if( !m_ActiveRecursiveDeletion )
    {
        doNextDeletion();
    }
}

void DialogDelete::onFileDeletionFailedSlot( QString path, DeleteFailureReason reason )
{
    //Show an error message
    qDebug() << "Failed to delete path " << path;
    QMessageBox errorBox( QMessageBox::Critical, "Failed to delete remote directory", "An error occurred while deleting remote directory" + path + "Reason: " + reason, QMessageBox::Ok );
    errorBox.exec();
    return;
}

void DialogDelete::onRecursiveDeleteCompletedSlot()
{
    m_ActiveRecursiveDeletion = false;
    doNextDeletion();
}

void DialogDelete::onDeletionCompleteSlot()
{
    ui->label->setText( "Nothing being deleted." );
    hide();
}

void DialogDelete::onCancelButtonReleasedSlot()
{
    ui->label->setText( "" );
    hide();
    emit cancelDeletionSignal( );
}

void DialogDelete::onDeletionTimeoutSlot()
{
    //Show an error message
    qDebug() << "Failed to delete path " << m_CurrentListing->Path();
    QMessageBox errorBox( QMessageBox::Critical, "Timeout deleting a remote path", "A timerout occurred while deleting " + m_CurrentListing->Path(), QMessageBox::Ok );
    errorBox.exec();
    return;
}

void DialogDelete::doNextDeletion()
{
    //Are we done?
    if( m_RemotePaths.empty() )
    {
        //It seems we are done
        hide();
        emit deletionCompletedSignal();
        return;
    }

    m_CurrentListing = m_RemotePaths.front();
    m_RemotePaths.pop_front();

    //Signal the deletion
    if( m_CurrentListing->Type() == DET_USERDIR )
    {
        m_ActiveRecursiveDeletion = true;
        m_DeletionThread.onDeleteRecursiveSlot( m_CurrentListing->Path() );
    }else if( m_CurrentListing->Type() == DET_FILE )
    {
        m_ActiveRecursiveDeletion = false;
        m_DeletionThread.onDeleteFileSlot( m_CurrentListing->Path() );
    }
}
