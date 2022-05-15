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
    Ui::ScanningWindow *ui;


public slots:
    void onNewDeviceDiscoveredSlot( QSharedPointer<AmigaHost> host );
    void onDeviceLeftSlot(  QSharedPointer<AmigaHost> host  );
    void onHostDoubleClickedSlot( QListWidgetItem *item );
    void onBrowserWindowDestroyedSlot();
    void onSystemTrayMenuItemSelected();
    void onSystemTrayIconClickedSlot( QSystemTrayIcon::ActivationReason reason );


private:
    DeviceDiscovery m_DeviceDiscovery;
    QMap<QString, MainWindow*> m_BrowserList;
    QSystemTrayIcon m_SystemTrayIcon;
    QMenu m_SystemTrayMenu;
    QMenu m_SystemTrayHostsMenu;
};

#endif // SCANNINGWINDOW_H
