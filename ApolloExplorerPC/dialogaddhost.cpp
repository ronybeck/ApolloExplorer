#include "dialogaddhost.h"
#include "ui_dialogaddhost.h"

DialogAddHost::DialogAddHost(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogAddHost)
{
    ui->setupUi(this);

    //Setup signals and slots
    connect( ui->comboBoxHardware, &QComboBox::currentIndexChanged, this, &DialogAddHost::onHardwareSelectedSlot );
}

DialogAddHost::~DialogAddHost()
{
    delete ui;
}

QSharedPointer<AmigaHost> DialogAddHost::getAmigaHost()
{
    //Get the definition of what the user put in the dialog
    QString name = ui->lineEditName->text();
    QString ipAddressText = ui->lineEditIPAddress->text();
    QString osVersion = ui->lineEditOSVersion->text();
    QString osName = ui->lineEditOSName->text();
    QString hardwareName = ui->comboBoxHardware->currentText();

    //We need to turn the text of the IP Address into something QT understands as an IP Address
    QHostAddress ipAddress;
    ipAddress.setAddress( ipAddressText );
    AmigaHost::HardwareType hardwareType = AmigaHost::hardwareTypeFromString( hardwareName );
    AmigaHost *host = new AmigaHost(99999999, name, osName, osVersion, hardwareType, ipAddress, true, this->parent() );

    return QSharedPointer<AmigaHost>( host );
}

void DialogAddHost::onHardwareSelectedSlot( int index ) {
    Q_UNUSED( index );
    QString hardwareString = ui->comboBoxHardware->currentText();
    AmigaHost::HardwareType type = AmigaHost::hardwareTypeFromString( hardwareString );
    QPixmap logo = AmigaHost::getPixmap( type );
    ui->labelLogo->setPixmap( logo );
}
