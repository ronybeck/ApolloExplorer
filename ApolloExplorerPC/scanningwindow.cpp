#include "scanningwindow.h"
#include "ui_scanningwindow.h"
#include <QPixmap>
#include <QWindow>

#include "AEUtils.h"

static QString getItemName( QString name, QHostAddress address )
{
    QString itemName = name + "\n" + address.toString();
    return itemName;
}

static QSharedPointer<AmigaHost> findHostByAddress( QString address, QMap<QString, QSharedPointer<AmigaHost>> mapping, bool& found )
{
    found = false;
    QMapIterator<QString,QSharedPointer<AmigaHost>> iter( mapping );
    while( iter.hasNext() )
    {
        auto nextEntry = iter.next();
        auto host = nextEntry.value();
        if( host->Address().toString() == address )
        {
            found = true;
            return host;
        }
    }
    return nullptr;
}

ScanningWindow::ScanningWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ScanningWindow),
    m_Settings( new QSettings( "ApolloTeam", "ApolloExplorer" ) ),
    m_StaticHosts( new QSettings( "ApolloTeam", "ApolloExplorerStaticHosts" ) ),
    m_DeviceDiscovery( m_Settings ),
    m_SystemTrayIcon( QPixmap( ":/browser/icons/VampireHW.png" ) ),
    m_DialogPreferences( m_Settings, this )
{
    ui->setupUi(this);

    //Signal Slots
    connect( &m_DeviceDiscovery, &DeviceDiscovery::hostAliveSignal, this, &ScanningWindow::onNewDeviceDiscoveredSlot );
    connect( &m_DeviceDiscovery, &DeviceDiscovery::hostDiedSignal, this, &ScanningWindow::onDeviceLeftSlot );
    connect( ui->listWidget, &QListWidget::itemDoubleClicked, this, &ScanningWindow::onHostDoubleClickedSlot );
    connect( ui->listWidget, &QListWidget::itemClicked, this, &ScanningWindow::onHostIconClickedSlot );
    connect( ui->checkBoxOpenAutomatically, &QCheckBox::released, this, &ScanningWindow::onAutoConnectCheckboxToggledSlot );
    connect( ui->pushButton, &QPushButton::released, this, &ScanningWindow::onConnectButtonReleasedSlot );
    connect( ui->pushButtonSettings, &QPushButton::released, &m_DialogPreferences, &DialogPreferences::show );
    connect( ui->pushButtonAbout, &QPushButton::released, &m_AboutDialog, &AboutDialog::show );
    connect( ui->pushButtonAdd, &QPushButton::released, this, &ScanningWindow::onAddHostReleasedSlot );

    //We want to forcibly remove the host from the device discovery sometimes
    connect( this, &ScanningWindow::ejectHostSignal, &m_DeviceDiscovery, &DeviceDiscovery::onEjectHostSlot );

    //Add a custom menu
    ui->listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listWidget, &QListWidget::customContextMenuRequested, this, &ScanningWindow::onContextMenuRequestedSlot);


    //Show the system try
    m_SystemTrayIcon.show();

    //Create a context menu for the system try
    m_SystemTrayIcon.setContextMenu( &m_SystemTrayMenu );
    m_SystemTrayMenu.addMenu( &m_SystemTrayHostsMenu );
    m_SystemTrayHostsMenu.setTitle( "Amiga Hosts" );
    m_SystemTrayMenu.addSeparator();
    connect( &m_SystemTrayIcon, &QSystemTrayIcon::activated, this, &ScanningWindow::onSystemTrayIconClickedSlot );
    //QAction *quitAction = m_SystemTrayMenu.addAction( "Quit", this, &ScanningWindow::close );

    //Restore the last window position
    m_Settings->beginGroup( SETTINGS_SCANNING_WINDOW );
    this->resize(
                m_Settings->value( SETTINGS_WINDOW_WIDTH, size().width() ).toInt(),
                m_Settings->value( SETTINGS_WINDOW_HEIGHT, size().height() ).toInt()
                );
    this->move(
                m_Settings->value( SETTINGS_WINDOW_POSX, pos().x() ).toInt(),
                m_Settings->value( SETTINGS_WINDOW_POSY, pos().y() ).toInt()
                );
    m_Settings->endGroup();

    //Show the "Whats New" dialog
    m_Settings->beginGroup( SETTINGS_SCANNING_WINDOW );
    QString seenWhatsNewVersion = m_Settings->value( SETTINGS_SEEN_WHATS_NEW_VERSION, "" ).toString();
    if( seenWhatsNewVersion != QString( VERSION_STRING ) )
    {
        m_DialogWhatsnew.show();
        m_Settings->setValue( SETTINGS_SEEN_WHATS_NEW_VERSION, VERSION_STRING );
    }
    m_Settings->endGroup();

    //Load the static hosts
    populateStaticHosts();
}

ScanningWindow::~ScanningWindow()
{
    m_SystemTrayMenu.setVisible( false );

    //Clean up menu
    QList<QAction*> actions = m_SystemTrayHostsMenu.actions();
    QListIterator<QAction*> actionIter( actions );
    while( actionIter.hasNext() )
    {
        QAction *action = actionIter.next();
        m_SystemTrayHostsMenu.removeAction( action );
        delete action;
    }
    delete ui;
}

void ScanningWindow::openNewHostWindow(QSharedPointer<AmigaHost> host)
{
    //Now open a browser window for this.
    QString addressString = host->Address().toString();

    //First see if we have a window for this address already
    if( m_BrowserList.contains( addressString ) )
    {
        MainWindow *mainWindow = m_BrowserList[ addressString ];
        mainWindow->raise();
        return;
    }

    MainWindow *newWindow = new MainWindow( m_Settings, host );
    newWindow->onConnectButtonReleasedSlot();
    newWindow->setConfirmWindowClose( false );
    newWindow->show();

    connect( newWindow, &MainWindow::browserWindowCloseSignal, this, &ScanningWindow::onBrowserWindowDestroyedSlot );
}

void ScanningWindow::populateStaticHosts()
{
    //Go through all of the groups and get the hosts out
    auto staticHostList = m_StaticHosts->childGroups();
    for( auto iter = staticHostList.begin(); iter != staticHostList.end(); iter++ ) {
        //Extract from the hosts file
        QString hostIP = (*iter);
        m_StaticHosts->beginGroup( hostIP );
        QString hostname = m_StaticHosts->value( STATIC_HOSTS_NAME, "INVALID" ).toString();
        QString osname = m_StaticHosts->value( STATIC_HOSTS_OS_NAME, "INVALID" ).toString();
        QString osversion = m_StaticHosts->value( STATIC_HOSTS_OS_VERSION, "INVALID" ).toString();
        QString hardwareName = m_StaticHosts->value( STATIC_HOSTS_HARDWARE_NAME, "INVALID" ).toString();

        //Form a host object
        AmigaHost::HardwareType hardwareType = AmigaHost::hardwareTypeFromString( hardwareName );
        QHostAddress ipAddress;
        ipAddress.setAddress( hostIP );
        AmigaHost *host = new AmigaHost(99999,hostname,osname,osversion,hardwareType, ipAddress, true, this );
        onNewDeviceDiscoveredSlot( QSharedPointer<AmigaHost>( host ) );
        m_StaticHosts->endGroup();
    }
}

void ScanningWindow::onNewDeviceDiscoveredSlot( QSharedPointer<AmigaHost> host )
{
    //Check that we don't already have this host
    if ( m_HostMap.contains( host->Address().toString() ) ) {
        return;
    }

    //Update the hostmap
    m_HostMap[ host->Address().toString() ] = host;

    //Create a new item for the browser
    QListWidgetItem *item = new QListWidgetItem();
    AmigaHost::HardwareType hardwareType =  host->Hardware();
    QString hardwareTypeName = AmigaHost::hardwareTypeAsString( hardwareType );
    item->setText( getItemName( host->Name(), host->Address() ) );
    QString hint( "Name: " + host->Name() + "\nOS: " + host->OsName() + " " + host->OsVersion() + "\nHardware: " + hardwareTypeName + "\nAddress: " + host->Address().toString() );
    item->setIcon( AmigaHost::getPixmap( hardwareType ) );
    item->setToolTip( hint );
    item->setData( Qt::UserRole, host->Address().toString() );

    ui->listWidget->addItem( item );

    //Add a new system try menu option
    QString actionLabel = host->Name() + "(" + host->Address().toString() + ")";
    QAction  *action = m_SystemTrayHostsMenu.addAction( QPixmap( ":/browser/icons/VampireHW.png" ), actionLabel, this, &ScanningWindow::onSystemTrayMenuItemSelected );
    action->setData( host->Address().toString() );

    //Check if we have a window for this device already.
    //If so, reenable it.
    QString address = host->Address().toString();
    if( m_BrowserList.contains( address ) )
    {
        MainWindow *mainWin = m_BrowserList[ address ];
        //mainWin->close();
        mainWin->onDeviceDiscoveredSlot();
    }else
    {
        //If this needs to be opened automatically, open it now
        m_Settings->beginGroup( "hosts" );
        m_Settings->beginGroup( host->Name() );
        bool openWindow = m_Settings->value( "AutoConnect", false ).toBool();
        m_Settings->endGroup();
        m_Settings->endGroup();
        if( openWindow )
        {
            openNewHostWindow( host );
        }

    }
}

void ScanningWindow::onDeviceLeftSlot( QSharedPointer<AmigaHost> host )
{
    QString itemName = getItemName( host->Name(), host->Address() );
    //auto items = ui->listWidget->findItems( itemName, Qt::MatchExactly );
    auto items = ui->listWidget->findItems( host->Address().toString(), Qt::MatchContains );

    //If this is the currently selected host, disable the system tab
    if( !m_SelectedHost.isNull() && ( m_SelectedHost->Name() == host->Name() ) )
    {
        ui->groupBoxDetails->setEnabled( false );
    }

    //Remove these items
    QListIterator<QListWidgetItem*> iter( items );
    while( iter.hasNext() )
    {
        qDebug() << "Removing " << itemName << " from the scan browser.";
        QListWidgetItem *item = iter.next();
        delete item;
    }

    //Which amiga is this?
    QString address = host->Address().toString();

    //Close the window
    if( m_BrowserList.contains( address ) )
    {
        MainWindow *mainWin = m_BrowserList[ address ];
        //mainWin->close();
        mainWin->onDeviceLeftSlot();
    }


    //remove the menu option
    QList<QAction*> actions = m_SystemTrayHostsMenu.actions();
    QListIterator<QAction*> actionIter( actions );
    while( actionIter.hasNext() )
    {
        QAction *action = actionIter.next();
        if( action->data().toString() == address )
        {
            m_SystemTrayHostsMenu.removeAction( action );
            delete action;
            break;
        }
    }

    //Remove it from the host listing
    m_HostMap.remove( host->Address().toString() );
}

void ScanningWindow::onHostDoubleClickedSlot( QListWidgetItem *item )
{
    //Now open a browser window for this.
    QString addressString = item->data( Qt::UserRole ).toString();

    //First see if we have a window for this address already
    if( m_BrowserList.contains( addressString ) )
    {
        MainWindow *mainWindow = m_BrowserList[ addressString ];
        mainWindow->raise();
        return;
    }

    //Ok, so we need to open a new browser window
    bool found;
    QSharedPointer<AmigaHost> host = findHostByAddress( addressString, m_HostMap, found );
    if( !found )
    {
        DBGLOG << "Couldn't find host with ip address " << addressString << " in the host map.";
        return;
    }

    MainWindow *newWindow = new MainWindow( m_Settings, host );
    newWindow->onConnectButtonReleasedSlot();
    newWindow->setConfirmWindowClose( false );
    newWindow->show();

    connect( newWindow, &MainWindow::browserWindowCloseSignal, this, &ScanningWindow::onBrowserWindowDestroyedSlot );

    m_BrowserList[ addressString ] = newWindow;
}

void ScanningWindow::onBrowserWindowDestroyedSlot()
{
    //Just free the window so we don't have a memory leak
    //delete QObject::sender();
    QObject::sender()->deleteLater();

    //Go through our list and remove the appropriate entry
    QMapIterator<QString,MainWindow*> iter( m_BrowserList );
    while( iter.hasNext() )
    {
        auto pair = iter.next();
        QString address = pair.key();
        MainWindow *mainWin = pair.value();
        if( mainWin == QObject::sender() )
        {
            m_BrowserList.remove( address );
            break;
        }
    }
}

void ScanningWindow::onSystemTrayMenuItemSelected()
{
    QAction *action = dynamic_cast<QAction*>( QObject::sender() );
    QString hostAddress = action->data().toString();
    qDebug() << "Host " << hostAddress << " selected";

    //See if we have a window open already for it
    if( m_BrowserList.contains( hostAddress ) )
    {
        MainWindow *mainWin = m_BrowserList[ hostAddress ];
        mainWin->raise();
        return;
    }

    //Ok, we need to open a new window
    bool found;
    QSharedPointer<AmigaHost> host = findHostByAddress( hostAddress, m_HostMap, found );
    if( !found )
    {
        DBGLOG << "Couldn't find host with ip address " << hostAddress << " in the host map.";
        return;
    }

    //Otherwise open one
    MainWindow *newWindow = new MainWindow( m_Settings, host );
    newWindow->onConnectButtonReleasedSlot();
    newWindow->setConfirmWindowClose( false );
    newWindow->show();
    connect( newWindow, &MainWindow::browserWindowCloseSignal, this, &ScanningWindow::onBrowserWindowDestroyedSlot );
    m_BrowserList[ hostAddress ] = newWindow;
}

void ScanningWindow::onSystemTrayIconClickedSlot( QSystemTrayIcon::ActivationReason reason )
{
    if( reason != QSystemTrayIcon::DoubleClick )
        return;

    if( isVisible() )
        setVisible( false );
    else
        setVisible( true );
}

void ScanningWindow::onHostIconClickedSlot( QListWidgetItem *item  )
{
    //First show the side bar
    ui->groupBoxDetails->setEnabled( true );

    //Get the host information
    QString address( item->data( Qt::UserRole ).toString() );
    m_SelectedHost = m_HostMap[ address ];

    //Set the details in the side bar
    AmigaHost::HardwareType hardwareType = m_SelectedHost->Hardware();
    QPixmap icon = AmigaHost::getPixmap( hardwareType );
    //QPixmap scaledIcon = icon.scaledToWidth( ui->labelIcon->width() );
    //ui->labelIcon->setPixmap( scaledIcon );
    ui->labelIcon->setPixmap( icon );
    ui->labelName->setText( "Name: " + m_SelectedHost->Name() );
    ui->labelHardware->setText( "Hardware: " + m_SelectedHost->HardwareName() );
    ui->labelOS->setText( "OS: " + m_SelectedHost->OsName() + " " + m_SelectedHost->OsVersion() );
    ui->labelIPAddress->setText( "IP: " + address );

    //Get the settings for this host
    m_Settings->beginGroup( SETTINGS_HOSTS );
    m_Settings->beginGroup( m_SelectedHost->Name() );
    ui->checkBoxOpenAutomatically->setChecked( m_Settings->value( "AutoConnect", false ).toBool() );
    m_Settings->endGroup();
    m_Settings->endGroup();
}

void ScanningWindow::onConnectButtonReleasedSlot()
{
    openNewHostWindow( m_SelectedHost );
}

void ScanningWindow::onAutoConnectCheckboxToggledSlot()
{
    //Find the settings for this host
    QString hostname = m_SelectedHost->Name();
    m_Settings->beginGroup( SETTINGS_HOSTS );
    m_Settings->beginGroup( hostname );
    m_Settings->setValue( "AutoConnect", ui->checkBoxOpenAutomatically->isChecked() );
    qDebug() << "Setting auto connect to " << m_Settings->value( "AutoConnect" ).toBool() << " for host " << hostname;
    m_Settings->endGroup();
    m_Settings->endGroup();
    m_Settings->sync();
}

void ScanningWindow::onAddHostReleasedSlot()
{
    if ( m_DialogAddHost.exec() == QDialog::Accepted ) {
        auto host = m_DialogAddHost.getAmigaHost();
        //Remove this from the local list
        onDeviceLeftSlot( host );

        //Now add it again so that the new details will be picked up.
        onNewDeviceDiscoveredSlot( host );

        //Add this host to the host list
        m_StaticHosts->beginGroup( host->Address().toString() );
        m_StaticHosts->setValue( STATIC_HOSTS_NAME, host->Name() );
        m_StaticHosts->setValue( STATIC_HOSTS_OS_NAME, host->OsName() );
        m_StaticHosts->setValue( STATIC_HOSTS_OS_VERSION, host->OsVersion() );
        m_StaticHosts->setValue( STATIC_HOSTS_HARDWARE_NAME, host->HardwareName() );
        m_StaticHosts->endGroup();
        m_StaticHosts->sync();


    }
}

void ScanningWindow::onDeleteHostReleasedSlot()
{
    //Get the host being deleted
    auto selectedList = ui->listWidget->selectedItems();
    for( auto iter = selectedList.begin(); iter != selectedList.end(); iter++ ) {
        auto item = (*iter);
        QString ipAddressString = item->data( Qt::UserRole ).toString();
        QSharedPointer<AmigaHost> host = m_HostMap[ ipAddressString ];
        onDeviceLeftSlot( host );

        //Now Remove it from the static host list
        m_StaticHosts->remove( host->Address().toString() );

        emit ScanningWindow::ejectHostSignal( host );
    }
}

void ScanningWindow::onContextMenuRequestedSlot(const QPoint &pos)
{
    //First, get the item the user clicked on
    QListWidgetItem *item = ui->listWidget->itemAt(pos);
    if (!item) return;  // No item at click position

    //Now get the host associated with it
    QString ipAddressString = item->data( Qt::UserRole ).toString();
    if ( !m_HostMap.contains( ipAddressString ) ) return;
    QSharedPointer<AmigaHost> host = m_HostMap[ ipAddressString ];

    //Ignore hosts which are not statically configured
    if ( !host->StaticlyConfiguredHost() )  return;

    //Now setup a menu
    QMenu menu(this);

    // Add actions
    QAction *deleteAction = menu.addAction("Delete");

    // Optional: pass item data to actions (e.g., via lambda)
    connect(deleteAction, &QAction::triggered, this, &ScanningWindow::onDeleteHostReleasedSlot );

    // Show menu at global position
    menu.exec( ui->listWidget->viewport()->mapToGlobal(pos));
}

void ScanningWindow::resizeEvent( QResizeEvent *event )
{
    QMainWindow::resizeEvent( event );

    //Change the settings
    m_Settings->beginGroup( SETTINGS_SCANNING_WINDOW );
    m_Settings->setValue( SETTINGS_WINDOW_WIDTH, event->size().width() );
    m_Settings->setValue( SETTINGS_WINDOW_HEIGHT, event->size().height() );
    m_Settings->endGroup();
    m_Settings->sync();
}

void ScanningWindow::moveEvent(QMoveEvent *event )
{
    QMainWindow::moveEvent( event );

    //Change the settings
    m_Settings->beginGroup( SETTINGS_SCANNING_WINDOW );
    m_Settings->setValue( SETTINGS_WINDOW_POSX, event->pos().x() );
    m_Settings->setValue( SETTINGS_WINDOW_POSY, event->pos().y() );
    m_Settings->endGroup();
    m_Settings->sync();
}
