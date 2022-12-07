#ifndef DIALOGDELETE_H
#define DIALOGDELETE_H

#include <QDialog>
#include <QSharedPointer>
#include <QAtomicInteger>
#include "deletionthread.h"
#include "directorylisting.h"

namespace Ui {
class DialogDelete;
}

class DialogDelete : public QDialog
{
    Q_OBJECT

public:
    explicit DialogDelete(QWidget *parent = nullptr);
    ~DialogDelete();

    void connectToHost( QHostAddress host, quint16 port );
    void disconnectFromhost();

public slots:
    void onDeleteRemotePathsSlot( QList<QSharedPointer<DirectoryListing>> remotePaths );
    void onCurrentFileBeingDeletedSlot( QString path );
    void onFileDeletionFailedSlot( QString path, DeleteFailureReason reason );
    void onRecursiveDeleteCompletedSlot();
    void onDeletionCompleteSlot();
    void onCancelButtonReleasedSlot();
    void onDeletionTimeoutSlot();

signals:
    void cancelDeletionSignal();
    void deletionCompletedSignal();

private:
    void doNextDeletion();

private:
    Ui::DialogDelete *ui;
    DeletionThread m_DeletionThread;
    QList<QSharedPointer<DirectoryListing>> m_RemotePaths;
    QSharedPointer<DirectoryListing> m_CurrentListing;
    QAtomicInteger<bool> m_ActiveRecursiveDeletion;
};

#endif // DIALOGDELETE_H
