#include "scanningwindow.h"
#include "ui_scanningwindow.h"

static QString getItemName( QString name, QHostAddress address )
{
    QString itemName = name + "\n" + address.toString();
    return itemName;
}

ScanningWindow::ScanningWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ScanningWindow),
    m_SystemTrayIcon( QPixmap( ":/browser/icons/VampireHW.png" ) )
{
    ui->setupUi(this);
    setWindowTitle( "VNet Scanner" );

    //Signal Slots
    connect( &m_DeviceDiscovery, &DeviceDiscovery::hostAliveSignal, this, &ScanningWindow::onNewDeviceDiscoveredSlot );
    connect( &m_DeviceDiscovery, &DeviceDiscovery::hostDiedSignal, this, &ScanningWindow::onDeviceLeftSlot );
    connect( ui->listWidget, &QListWidget::itemDoubleClicked, this, &ScanningWindow::onHostDoubleClickedSlot );

    //Show the system try
    m_SystemTrayIcon.show();

    //Create a context menu for the system try
    m_SystemTrayIcon.setContextMenu( &m_SystemTrayMenu );
    m_SystemTrayMenu.addMenu( &m_SystemTrayHostsMenu );
    m_SystemTrayHostsMenu.setTitle( "VNhet Hosts" );
    m_SystemTrayMenu.addSeparator();
    connect( &m_SystemTrayIcon, &QSystemTrayIcon::activated, this, &ScanningWindow::onSystemTrayIconClickedSlot );
    QAction *quitAction = m_SystemTrayMenu.addAction( "Quit", this, &ScanningWindow::close );
    //quitAction->setMenuRole( QAction::QuitRole );
    //quitAction->setPriority( QAction::HighPriority );
}

ScanningWindow::~ScanningWindow()
{
    m_SystemTrayMenu.setVisible( false );
    delete ui;
}

void ScanningWindow::onNewDeviceDiscoveredSlot( QSharedPointer<VNetHost> host )
{
    //Create a new item for the browser
    QListWidgetItem *item = new QListWidgetItem();
    item->setText( getItemName( host->Name(), host->Address() ) );
    QString hint( "Name: " + host->Name() + "\nOS: " + host->OsName() + " " + host->OsVersion() + "\nHardware: " + host->Hardware() + "\nAddress: " + host->Address().toString() );
    if( host->Hardware().contains( "V2", Qt::CaseInsensitive ) ||
        host->Hardware().contains( "V4", Qt::CaseInsensitive ) ||
        host->Hardware().contains( "V500", Qt::CaseInsensitive ) ||
        host->Hardware().contains( "V600", Qt::CaseInsensitive ) ||
        host->Hardware().contains( "V1200", Qt::CaseInsensitive ))
    {
        item->setIcon( QPixmap( ":/browser/icons/VampireHW.png" ) );
    }
    else if( host->Hardware().contains( "FB", Qt::CaseInsensitive ) || host->Hardware().contains( "FB500", Qt::CaseInsensitive ))
    {
        item->setIcon( QPixmap( ":/browser/icons/FirebirdHW.png" ) );
    }
    else if( host->Hardware().contains( "Icedrake", Qt::CaseInsensitive ))
    {
        item->setIcon( QPixmap( ":/browser/icons/IcedrakeHW.png" ) );
    }
    else
    {
        item->setIcon( QPixmap( ":/browser/icons/CommodoreHW.png" ) );
    }
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
    }
}

void ScanningWindow::onDeviceLeftSlot( QSharedPointer<VNetHost> host )
{
    QString itemName = getItemName( host->Name(), host->Address() );
    auto items = ui->listWidget->findItems( itemName, Qt::MatchExactly );

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

        MainWindow *newWindow = new MainWindow();
        newWindow->onSetHostSlot( QHostAddress( addressString ) );
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

    //Otherwise open one
    MainWindow *newWindow = new MainWindow();
    newWindow->onSetHostSlot( QHostAddress( hostAddress ) );
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
