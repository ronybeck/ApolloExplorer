#ifndef SCANNINGWINDOW_H
#define SCANNINGWINDOW_H

#include "mainwindow.h"
#include "devicediscovery.h"
#include "AEUtils.h"

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

    //Window Manipulation
    void resizeEvent( QResizeEvent *event ) override;
    void moveEvent(QMoveEvent *event) override;

private:
    QSharedPointer<QSettings> m_Settings;
    DeviceDiscovery m_DeviceDiscovery;
    QMap<QString, MainWindow*> m_BrowserList;
    QMap<QString, QSharedPointer<AmigaHost>> m_HostMap;
    QSystemTrayIcon m_SystemTrayIcon;
    QMenu m_SystemTrayMenu;
    QMenu m_SystemTrayHostsMenu;
    QSharedPointer<AmigaHost> m_SelectedHost;
};

#endif // SCANNINGWINDOW_H
