#ifndef REMOTEFILETABLEVIEW_H
#define REMOTEFILETABLEVIEW_H

#include "remotefilemimedata.h"
#include "dialogdownloadfile.h"
#include "dialoguploadfile.h"

#include <QTableView>
#include <QObject>

class RemoteFileTableView : public QTableView
{
    Q_OBJECT
public:
    RemoteFileTableView();

    void dragLeaveEvent( QDragLeaveEvent *e ) override;
    void dragEnterEvent( QDragEnterEvent *event ) override;
    void dropEvent( QDropEvent *e ) override;
    void dragMoveEvent( QDragMoveEvent *e ) override;
    void startDrag(Qt::DropActions supportedActions) override;
    void mouseMoveEvent( QMouseEvent *e ) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

    QList<QSharedPointer<DirectoryListing>> getSelectedItems();

    void setDownloadDialog( QSharedPointer<DialogDownloadFile> dialog );
    void setUploadDialog( QSharedPointer<DialogUploadFile> dialog );

signals:
    void itemsDoubleClicked( QList<QSharedPointer<DirectoryListing>> items );

public slots:
    void onItemDoubleClicked( const QModelIndex &index );

private:
    QSharedPointer<DialogDownloadFile> m_DownloadDialog;
    QSharedPointer<DialogUploadFile> m_UploadDialog;
    QList<QDrag*> m_QDragList;
};

#endif // REMOTEFILETABLEVIEW_H
