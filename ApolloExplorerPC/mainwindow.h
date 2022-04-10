#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QAction>
#include <QMutexLocker>
#include <QAtomicInt>

#include "protocolhandler.h"
#include "dialogdownloadfile.h"
#include "dialoguploadfile.h"
#include "dialogconsole.h"
#include "dialogdelete.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

    typedef enum
    {
        AS_Unknown,
        AS_SUCCESS,
        AS_FAILED,
    } AcknowledgeState;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void closeEvent (QCloseEvent *event) override;

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
    void onBrowserItemDoubleClickSlot();
    void onDrivesItemSelectedSlot();
    void onUpButtonReleasedSlot();
    void onPathEditFinishedSlot();
    void showContextMenu( QPoint );
    void onSetHostSlot( QHostAddress host );
    void onAbortDeletionRequestedSlot();

    //Context Menu
    void onRunCLIHereSlot();
    void onMkdirSlot();
    void onRebootSlot();
    void onDeleteSlot();
    void onDownloadSelectedSlot();

    //Network slots
    void onConnectedToHostSlot();
    void onDisconnectedFromHostSlot();
    void onIncomingByteCountUpdateSlot( quint32 bytes );
    void onOutgoingByteCountUpdateSlot( quint32 bytes );

    //SCanner Slots
    void onDeviceLeftSlot();
    void onDeviceDiscoveredSlot();

    //Protocol slots
    void onServerVersion( quint8 major, quint8 minor, quint8 revision );
    void onDirectoryListingUpdateSlot( QSharedPointer<DirectoryListing> newListing );
    void onVolumeListUpdateSlot( QStringList volumes );
    void onAcknowledgeSlot( quint8 responseCode );

    //Timer slots
    void onThroughputTimerExpired();
    void onReconnectTimerExpired();

    //Drag and Drop
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;
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
    QStringList m_Volumes;
    AcknowledgeState m_AcknowledgeState;
    QTimer m_ReconnectTimer;

    //Preferences
    bool m_ConfirmWindowClose;
    bool m_HideInfoFiles;
    bool m_ShowFileSizes;
    bool m_ListViewEnabled;

    //File downloading
    DialogDownloadFile m_DialogDownloadFile;
    DialogUploadFile m_DialogUploadFile;
    DialogDelete m_DialogDelete;

    //Throughput book keeping
    quint32 m_IncomingByteCount;
    quint32 m_OutgoingByteCount;
    QTimer m_ThroughputTimer;

    //Mutex
    QMutex m_Mutex;
    QAtomicInteger<bool> m_AbortDeleteRequested;
};
#endif // MAINWINDOW_H
