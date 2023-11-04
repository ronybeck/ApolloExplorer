#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QAction>
#include <QMutexLocker>
#include <QAtomicInt>
#include <QSharedPointer>
#include <QSettings>

#include "amigahost.h"
#include "protocolhandler.h"
#include "dialogdownloadfile.h"
#include "dialoguploadfile.h"
#include "dialogconsole.h"
#include "dialogdelete.h"
#include "dialogpreferences.h"
#include "aboutdialog.h"
#include "remotefiletableview.h"
#include "remotefilelistview.h"
#include "remotefiletablemodel.h"
#include "diskvolume.h"
#include "iconcache.h"
#include "../AmigaIconReader/AmigaInfoFile.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

    typedef enum
    {
        VIEW_TABLE,
        VIEW_LIST
    } ViewType;

public:
    explicit MainWindow( QSharedPointer<QSettings> settings, QSharedPointer<AmigaHost> amigaHost, QWidget *parent = nullptr);
    ~MainWindow();

    void closeEvent (QCloseEvent *event) override;

    //Window Manipulation
    void resizeEvent( QResizeEvent *event ) override;
    void moveEvent(QMoveEvent *event) override;

    bool ConfirmWindowClose() const;
    void setConfirmWindowClose(bool newConfirmWindowClose);

public slots:
    //Gui slots
    void onConnectButtonReleasedSlot();
    void onDisconnectButtonReleasedSlot();
    void onShowDrivesToggledSlot( bool enabled );
    void onListModeToggledSlot(bool enabled);
    void onShowInfoFilesToggledSlot(bool enabled);
    void onRefreshButtonReleasedSlot();
    //void onBrowserItemDoubleClickSlot();
    void onBrowserItemsDoubleClickedSlot( QList<QSharedPointer<DirectoryListing>> directoryListings );
    void onDrivesItemSelectedSlot();
    void onUpButtonReleasedSlot();
    void onPathEditFinishedSlot();
    void showContextMenu( QPoint );
    //void onSetHostSlot( QHostAddress host );
    void onAbortDeletionRequestedSlot();
    void onIconUpdateSlot( QString filePath, QSharedPointer<AmigaInfoFile> icon );

    //Menu itmes
    void onSettingsMenuItemClickedSlot();

    //Context Menu
    void onRunCLIHereSlot();
    void onMkdirSlot();
    void onRebootSlot();
    void onDeleteSlot();
    void onAboutSlot();
    void onRenameSlot();
    void onDownloadSelectedSlot();
    void onInformationSelectedSlot();

    //Network slots
    void onConnectedToHostSlot();
    void onDisconnectedFromHostSlot();
    void onServerClosedConnectionSlot( QString message );
    void onIncomingByteCountUpdateSlot( quint32 bytes );
    void onOutgoingByteCountUpdateSlot( quint32 bytes );

    //SCanner Slots
    void onDeviceLeftSlot();
    void onDeviceDiscoveredSlot();

    //Protocol slots
    void onServerVersion( quint8 major, quint8 minor, quint8 revision );
    void onDirectoryListingUpdateSlot( QSharedPointer<DirectoryListing> newListing );
    void onVolumeListUpdateSlot( QList<QSharedPointer<DiskVolume>> volumes );
    void onAcknowledgeSlot( quint8 responseCode );

    //Timer slots
    void onThroughputTimerExpired();
    void onReconnectTimerExpired();
    void onRefreshVolumesTimerExpired();

    //Drag and Drop
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragLeaveEvent( QDragLeaveEvent *e ) override;
    void dragMoveEvent( QDragMoveEvent *e ) override;
    //void dragEvent(QDragEnterEvent *e);

signals:
    void connectToHostSignal( QHostAddress server, quint16 port );
    void disconnectFromHostSignal();
    void sendProtocolMessageSignal( ProtocolMessage_t *message );
    void getRemoteDirectorySignal( QString remoteDirectory );
    void createRemoteDirectorySignal( QString remoteDirectory );
    void deleteRemoteDirectorySignal( QString remoteDirectory );
    void browserWindowCloseSignal();
    void currentFileBeingDeleted( QString filepath );
    void deletionCompletedSignal();
    void retrieveIconSignal( QString filepath );

private:
    void updateFilebrowser();
    void updateDrivebrowser();

    void deleteDirectoryRecursive( QString remotePath, bool &error );

    //Manage the downloading of directories
    QList<QPair<QString,QString>> downloadPrepareLocalDirectory( QString localPath, QString remotePath, bool &error );
    QList<QPair<QString,QString>> uploadPrepareRemoteDirctory( QString localPath, QString remotePath, bool &error );

private:
    Ui::MainWindow *ui;
    ProtocolHandler m_ProtocolHandler;
    QMap<QString, QSharedPointer<DirectoryListing>> m_DirectoryListings;
    QList<QSharedPointer<DiskVolume>> m_Volumes;
    ProtocolHandler::AcknowledgeState m_AcknowledgeState;
    QTimer m_ReconnectTimer;
    QTimer m_VolumeRefreshTimer;
    QSharedPointer<AmigaHost> m_AmigaHost;

    //Custom Views
    RemoteFileTableView *m_FileTableView;
    RemoteFileListView *m_FileListView;
    RemoteFileTableModel *m_FileTableModel;

    //Preferences
    QSharedPointer<QSettings> m_Settings;
    DialogPreferences m_DialogPreferences;
    bool m_ConfirmWindowClose;
    bool m_HideInfoFiles;
    bool m_ShowFileSizes;
    ViewType m_ViewType;

    //File downloading
    QSharedPointer<DialogDownloadFile> m_DialogDownloadFile;
    QSharedPointer<DialogUploadFile> m_DialogUploadFile;
    DialogDelete m_DialogDelete;

    //Icon cache
    IconCache m_IconCache;

    //About Dialog
    AboutDialog m_AboutDialog;

    //Throughput book keeping
    quint32 m_IncomingByteCount;
    quint32 m_OutgoingByteCount;
    QTimer m_ThroughputTimer;

    //Mutex
    QMutex m_Mutex;
    QAtomicInteger<bool> m_AbortDeleteRequested;
};
#endif // MAINWINDOW_H
