#ifndef SCANNINGWINDOW_H
#define SCANNINGWINDOW_H

#include "mainwindow.h"
#include "devicediscovery.h"

#include <QMainWindow>
#include <QMap>
#include <QSharedPointer>
#include <QHostAddress>
#include <QListWidget>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QSettings>
#include <QResizeEvent>
#include <QMoveEvent>

#include "dialogpreferences.h"
#include "aboutdialog.h"
#include "dialogwhatsnew.h"
#include "dialogaddhost.h"

namespace Ui {
class ScanningWindow;
}

class ScanningWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ScanningWindow(QWidget *parent = nullptr);
    ~ScanningWindow();

private:
    void openNewHostWindow( QSharedPointer<AmigaHost> host );
    void populateStaticHosts();

private:
    Ui::ScanningWindow *ui;


public slots:
    void onNewDeviceDiscoveredSlot( QSharedPointer<AmigaHost> host );
    void onDeviceLeftSlot(  QSharedPointer<AmigaHost> host  );
    void onHostDoubleClickedSlot( QListWidgetItem *item );
    void onBrowserWindowDestroyedSlot();
    void onSystemTrayMenuItemSelected();
    void onSystemTrayIconClickedSlot( QSystemTrayIcon::ActivationReason reason );
    void onHostIconClickedSlot( QListWidgetItem *item );
    void onConnectButtonReleasedSlot();
    void onAutoConnectCheckboxToggledSlot();
    void onAddHostReleasedSlot();
    void onDeleteHostReleasedSlot();
    void onContextMenuRequestedSlot(const QPoint &pos);

    //Window Manipulation
    void resizeEvent( QResizeEvent *event ) override;
    void moveEvent(QMoveEvent *event) override;

signals:
    void ejectHostSignal( QSharedPointer<AmigaHost> );

private:
    QSharedPointer<QSettings> m_Settings;
    QSharedPointer<QSettings> m_StaticHosts;
    DeviceDiscovery m_DeviceDiscovery;
    QMap<QString, MainWindow*> m_BrowserList;
    QMap<QString, QSharedPointer<AmigaHost>> m_HostMap;
    QSystemTrayIcon m_SystemTrayIcon;
    QMenu m_SystemTrayMenu;
    QMenu m_SystemTrayHostsMenu;
    QSharedPointer<AmigaHost> m_SelectedHost;
    DialogPreferences m_DialogPreferences;
    AboutDialog m_AboutDialog;
    DialogWhatsNew m_DialogWhatsnew;
    DialogAddHost m_DialogAddHost;
};

#endif // SCANNINGWINDOW_H
