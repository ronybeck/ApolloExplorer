#ifndef REMOTEFILELISTVIEW_H
#define REMOTEFILELISTVIEW_H

#include "remotefiletablemodel.h"
#include "dialogdownloadfile.h"
#include "dialoguploadfile.h"

#include <QListView>
#include <QTimer>
#include <QObject>
#include <QSharedPointer>
#include <QSettings>

class RemoteFileListView : public QListView
{
    Q_OBJECT
public:
    RemoteFileListView( QWidget *parent );
    ~RemoteFileListView();


    void dragLeaveEvent( QDragLeaveEvent *e ) override;
    void dragEnterEvent( QDragEnterEvent *e ) override;
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

#endif // REMOTEFILELISTVIEW_H
