#include "dialogfileinfo.h"
#include "ui_dialogfileinfo.h"
#include "AEUtils.h"

DialogFileInfo::DialogFileInfo( QSharedPointer<DirectoryListing> directoryListing, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogFileInfo),
    m_DirectoryListing( directoryListing )
{
    ui->setupUi(this);

    //Set the file path and file size
    ui->groupBox->setTitle( m_DirectoryListing->Path() );
    this->setWindowTitle( m_DirectoryListing->Path() );
    ui->labelFileSize->setText( "Size: " + prettyFileSize( m_DirectoryListing->Size(), true ) );

    //Get the infor file
    QSharedPointer<AmigaInfoFile> infoFile = m_DirectoryListing->getAmigaInfoFile();

    //Check if we have an amiga info file to read.  If not, just set some dummy values
    if( infoFile.isNull() )
    {
        ui->lineEditDefaultTool->setText( "Not Available" );
        ui->lineEditStackSize->setText( "Not Available" );
        ui->lineEditPriority->setText( "Not Available" );
        ui->comboBox->setCurrentIndex( 0 );
        ui->plainTextEditToolTypes->appendPlainText( "Not Available" );
        return;
    }

    //Populate the text files
    ui->lineEditDefaultTool->setText( infoFile->getDefaultTool() );
    ui->lineEditStackSize->setText( QString::number( infoFile->getStackSize() ) );
    ui->lineEditPriority->setText( QString::number( infoFile->getPriority() ) );
    ui->comboBox->setCurrentIndex( infoFile->getType() );

    //Now get the tool types
    QVector<QString> toolTypes = infoFile->getToolTypes();
    for( QVector<QString>::Iterator iter = toolTypes.begin(); iter != toolTypes.end(); iter++ )
    {
        QString entry = *iter;
        ui->plainTextEditToolTypes->appendPlainText( entry );
    }

    //Now add the image
    int height = ui->labelIcon->height();
    QPixmap iconImage =  QPixmap::fromImage( infoFile->getBestImage2().scaledToHeight( height, Qt::SmoothTransformation ) );
    ui->labelIcon->setPixmap( iconImage );

    //We are done here
}

DialogFileInfo::~DialogFileInfo()
{
    delete ui;
}
