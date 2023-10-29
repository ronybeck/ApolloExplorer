#ifndef REMOTEFILETABLEVIEW_H
#define REMOTEFILETABLEVIEW_H

#include "remotefilemimedata.h"
#include "remotefiletablemodel.h"
#include "dialogdownloadfile.h"
#include "dialoguploadfile.h"

#include <QTableView>
#include <QTimer>
#include <QObject>
#include <QSharedPointer>
#include <QSettings>

class RemoteFileTableView : public QTableView
{
    Q_OBJECT
public:
    RemoteFileTableView( QWidget *parent );
    ~RemoteFileTableView();

    void dragLeaveEvent( QDragLeaveEvent *e ) override;
    void dragEnterEvent( QDragEnterEvent *event ) override;
    void dropEvent( QDropEvent *e ) override;
    void dragMoveEvent( QDragMoveEvent *e ) override;
    void startDrag(Qt::DropActions supportedActions) override;
    void mouseMoveEvent( QMouseEvent *e ) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    //void leaveEvent( QEvent *event ) override;


    QList<QSharedPointer<DirectoryListing>> getSelectedItems();

    void setDownloadDialog( QSharedPointer<DialogDownloadFile> dialog );
    void setUploadDialog( QSharedPointer<DialogUploadFile> dialog );
    void setSettings( QSharedPointer<QSettings> settings );

signals:
    void itemsDoubleClicked( QList<QSharedPointer<DirectoryListing>> items );
    void downloadViaDownloadDialogSignal();

public slots:
    void onItemDoubleClicked( const QModelIndex &index );
    void dropTimerTimeoutSlot();

private:
    QSharedPointer<DialogDownloadFile> m_DownloadDialog;
    QSharedPointer<DialogUploadFile> m_UploadDialog;
    QList<QDrag*> m_QDragList;
    QDrag *m_CurrentDrag;
    QTimer m_DropTimer;
    QSharedPointer<QSettings> m_Settings;
    RemoteFileTableModel *m_TemporaryModel;
};

#endif // REMOTEFILETABLEVIEW_H
