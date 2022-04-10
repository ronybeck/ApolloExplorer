#include "dialogdelete.h"
#include "ui_dialogdelete.h"

DialogDelete::DialogDelete(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogDelete)
{
    ui->setupUi(this);

    connect( ui->pushButton, &QPushButton::released, this, &DialogDelete::onCancelButtonReleasedSlot );
}

DialogDelete::~DialogDelete()
{
    delete ui;
}

void DialogDelete::onCurrentFileBeingDeletedSlot( QString filepath )
{
    ui->label->setText( "Deleting: " + filepath );
}

void DialogDelete::onDeletionCompleteSlot()
{
    ui->label->setText( "Nothing being deleted." );
    hide();
}

void DialogDelete::onCancelButtonReleasedSlot()
{
    ui->label->setText( "" );
    hide();
    emit cancelDeletionSignal( );
}
