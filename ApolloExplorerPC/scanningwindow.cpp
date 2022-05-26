#include "scanningwindow.h"
#include "ui_scanningwindow.h"
#include <QPixmap>
#include <QWindow>

static QPixmap getPixmap( QSharedPointer<AmigaHost> host )
{
    if( host->Hardware().contains( "V2", Qt::CaseInsensitive ) ||
        host->Hardware().contains( "V4", Qt::CaseInsensitive ) ||
        host->Hardware().contains( "V500", Qt::CaseInsensitive ) ||
        host->Hardware().contains( "V600", Qt::CaseInsensitive ) ||
        host->Hardware().contains( "V1200", Qt::CaseInsensitive ))
    {
        return QPixmap( ":/browser/icons/VampireHW.png" );
    }
    else if( host->Hardware().contains( "FB", Qt::CaseInsensitive ) || host->Hardware().contains( "FB500", Qt::CaseInsensitive ))
    {
        return QPixmap( ":/browser/icons/FirebirdHW.png" );
    }
    else if( host->Hardware().contains( "Icedrake", Qt::CaseInsensitive ))
    {
        return QPixmap( ":/browser/icons/IcedrakeHW.png" );
    }

    return QPixmap( ":/browser/icons/CommodoreHW.png" );
}

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
    m_SystemTrayIcon( QPixmap( ":/browser/icons/VampireHW.png" ) )
{
    ui->setupUi(this);
    setWindowTitle( "ApolloExplorer Scanner" );

    //Signal Slots
    connect( &m_DeviceDiscovery, &DeviceDiscovery::hostAliveSignal, this, &ScanningWindow::onNewDeviceDiscoveredSlot );
    connect( &m_DeviceDiscovery, &DeviceDiscovery::hostDiedSignal, this, &ScanningWindow::onDeviceLeftSlot );
    connect( ui->listWidget, &QListWidget::itemDoubleClicked, this, &ScanningWindow::onHostDoubleClickedSlot );
    connect( ui->listWidget, &QListWidget::itemClicked, this, &ScanningWindow::onHostIconClickedSlot );
    connect( ui->checkBoxOpenAutomatically, &QCheckBox::released, this, &ScanningWindow::onAutoConnectCheckboxToggledSlot );
    connect( ui->pushButton, &QPushButton::released, this, &ScanningWindow::onConnectButtonReleasedSlot );

    //Show the system try
    m_SystemTrayIcon.show();

    //Create a context menu for the system try
    m_SystemTrayIcon.setContextMenu( &m_SystemTrayMenu );
    m_SystemTrayMenu.addMenu( &m_SystemTrayHostsMenu );
    m_SystemTrayHostsMenu.setTitle( "VNhet Hosts" );
    m_SystemTrayMenu.addSeparator();
    connect( &m_SystemTrayIcon, &QSystemTrayIcon::activated, this, &ScanningWindow::onSystemTrayIconClickedSlot );
    QAction *quitAction = m_SystemTrayMenu.addAction( "Quit", this, &ScanningWindow::close );

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
}

ScanningWindow::~ScanningWindow()
{
    m_SystemTrayMenu.setVisible( false );
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

void ScanningWindow::onNewDeviceDiscoveredSlot( QSharedPointer<AmigaHost> host )
{
    //Update the hostmap
    m_HostMap[ host->Address().toString() ] = host;

    //Create a new item for the browser
    QListWidgetItem *item = new QListWidgetItem();
    item->setText( getItemName( host->Name(), host->Address() ) );
    QString hint( "Name: " + host->Name() + "\nOS: " + host->OsName() + " " + host->OsVersion() + "\nHardware: " + host->Hardware() + "\nAddress: " + host->Address().toString() );
    item->setIcon( getPixmap( host ) );
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
    auto items = ui->listWidget->findItems( itemName, Qt::MatchExactly );

    //If this is the currently selected host, disable the system tab
    if( m_SelectedHost->Name() == host->Name() )
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
            return;
        }
    }
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
    QPixmap icon = getPixmap( m_SelectedHost );
    //QPixmap scaledIcon = icon.scaledToWidth( ui->labelIcon->width() );
    //ui->labelIcon->setPixmap( scaledIcon );
    ui->labelIcon->setPixmap( icon );
    ui->labelName->setText( "Name: " + m_SelectedHost->Name() );
    ui->labelHardware->setText( "Hardware: " + m_SelectedHost->Hardware() );
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
