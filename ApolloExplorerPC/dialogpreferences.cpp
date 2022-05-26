#include "dialogpreferences.h"
#include "ui_dialogpreferences.h"

DialogPreferences::DialogPreferences( QSharedPointer<QSettings> settings, QWidget *parent ) :
    QDialog(parent),
    ui(new Ui::DialogPreferences),
    m_Settings( settings )
{
    ui->setupUi(this);

    //Restore the current values
    m_Settings->beginGroup( SETTINGS_BROWSER );
    ui->comboBox->setCurrentText( m_Settings->value( SETTINGS_BROWSER_DOUBLECLICK_ACTION, SETTINGS_IGNORE ).toString() );
    m_Settings->endGroup();
    m_Settings->sync();

    //Signals and Slots
    connect( ui->comboBox, &QComboBox::currentTextChanged, this, &DialogPreferences::onDoubleClickComboBoxChanged );
}

DialogPreferences::~DialogPreferences()
{
    delete ui;
}

void DialogPreferences::onDoubleClickComboBoxChanged( const QString selection )
{
    DBGLOG << "Selected " << selection;
    m_Settings->beginGroup( SETTINGS_BROWSER );
    m_Settings->setValue( SETTINGS_BROWSER_DOUBLECLICK_ACTION, selection );
    m_Settings->endGroup();
    m_Settings->sync();
}
