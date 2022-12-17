#include "dialogpreferences.h"
#include "ui_dialogpreferences.h"

#include "AEUtils.h"

DialogPreferences::DialogPreferences( QSharedPointer<QSettings> settings, QWidget *parent ) :
    QDialog(parent),
    ui(new Ui::DialogPreferences),
    m_Settings( settings )
{
    ui->setupUi(this);

    //Restore the current values
    m_Settings->beginGroup( SETTINGS_BROWSER );
    ui->comboBox->setCurrentText( m_Settings->value( SETTINGS_BROWSER_DOUBLECLICK_ACTION, SETTINGS_IGNORE ).toString() );
    ui->spinBoxDeleteDelay->setValue( m_Settings->value( SETTINGS_BROWSER_DELAY_BETWEEN_DELETES, 100 ).toInt() );
    ui->comboBoxDefaultSort->setCurrentText( m_Settings->value( SETTINGS_SORTING_DEFAULT, SETTINGS_SORTING_TYPE ).toString() );
    ui->checkBoxIgnoreCase->setChecked( m_Settings->value( SETTINGS_SORTING_IGNORE_CASE, false ).toBool() );
    m_Settings->endGroup();
    m_Settings->beginGroup( SETTINGS_GENERAL );
    ui->spinBoxHelloTimeout->setValue( m_Settings->value( SETTINGS_HELLO_TIMEOUT, 10 ).toInt() );
    m_Settings->endGroup();
    m_Settings->sync();

    //Signals and Slots
    connect( ui->comboBox, &QComboBox::currentTextChanged, this, &DialogPreferences::onDoubleClickComboBoxChangedSlot );
    connect( ui->spinBoxDeleteDelay, QOverload<int>::of(&QSpinBox::valueChanged), this, &DialogPreferences::onDelayBetweenDeletesChangedSlot );
    connect( ui->spinBoxHelloTimeout, QOverload<int>::of(&QSpinBox::valueChanged), this, &DialogPreferences::onHelloTimeoutChangedSlot );
    connect( ui->spinBoxDNDSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &DialogPreferences::onDNDSizeChangedSlot );
    connect( ui->comboBoxDNDOperation, &QComboBox::currentTextChanged, this, &DialogPreferences::onDNDOperationComboboxChangedSlot );
    connect( ui->comboBoxDefaultSort, &QComboBox::currentTextChanged, this, &DialogPreferences::onDefaultSortCombobBoxChangedSlot );
    connect( ui->checkBoxIgnoreCase, &QCheckBox::clicked, this, &DialogPreferences::onIgnoreCaseCheckBoxClickedSlot );
}

DialogPreferences::~DialogPreferences()
{
    delete ui;
}

void DialogPreferences::onDoubleClickComboBoxChangedSlot( const QString selection )
{
    DBGLOG << "Selected " << selection;
    m_Settings->beginGroup( SETTINGS_BROWSER );
    m_Settings->setValue( SETTINGS_BROWSER_DOUBLECLICK_ACTION, selection );
    m_Settings->endGroup();
    m_Settings->sync();
}

void DialogPreferences::onDelayBetweenDeletesChangedSlot( int newValue )
{
    DBGLOG << "Delay between deletes set to " << newValue;
    m_Settings->beginGroup( SETTINGS_BROWSER );
    m_Settings->setValue( SETTINGS_BROWSER_DELAY_BETWEEN_DELETES, newValue );
    m_Settings->endGroup();
    m_Settings->sync();
}

void DialogPreferences::onHelloTimeoutChangedSlot(int newValue)
{
    DBGLOG << "Hello timeout set to " << newValue;
    m_Settings->beginGroup( SETTINGS_GENERAL );
    m_Settings->setValue( SETTINGS_HELLO_TIMEOUT, newValue );
    m_Settings->endGroup();
    m_Settings->sync();
}

void DialogPreferences::onDNDSizeChangedSlot(int newValue)
{
    DBGLOG << "DND Size set to " << newValue;
    m_Settings->beginGroup( SETTINGS_GENERAL );
    m_Settings->setValue( SETTINGS_DND_SIZE, newValue );
    m_Settings->endGroup();
    m_Settings->sync();
}

void DialogPreferences::onDNDOperationComboboxChangedSlot(const QString selection)
{
    DBGLOG << "DND Operations " << selection;
    m_Settings->beginGroup( SETTINGS_BROWSER );
    m_Settings->setValue( SETTINGS_DND_OPERATION, selection );
    m_Settings->endGroup();
    m_Settings->sync();
}

void DialogPreferences::onDefaultSortCombobBoxChangedSlot(const QString selection)
{
    DBGLOG << "Selected " << selection;
    m_Settings->beginGroup( SETTINGS_BROWSER );
    m_Settings->setValue( SETTINGS_SORTING_DEFAULT, selection );
    m_Settings->endGroup();
    m_Settings->sync();
}

void DialogPreferences::onIgnoreCaseCheckBoxClickedSlot(const bool checked)
{
    DBGLOG << "Selected " << (checked ? "TRUE" : "FALSE");
    m_Settings->beginGroup( SETTINGS_BROWSER );
    m_Settings->setValue( SETTINGS_SORTING_IGNORE_CASE, checked );
    m_Settings->endGroup();
    m_Settings->sync();
}
