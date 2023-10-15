#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QErrorMessage>
#include <QPixmap>
#include <QPainter>
#include <QPaintEngine>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_actionOpen_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Image"), "~/", tr("Image Files (*.info)"));

    if( !m_Icon.loadFile( fileName ) )
    {
        QErrorMessage msgBox( this );
        msgBox.showMessage("Failed to open file " + fileName );
        msgBox.exec();
    }

    //Now populate our gui
    quint8 depth;
    quint32 width, height;
    m_Icon.getOS2IconParameters( depth, width, height );
    ui->lineEditFilename->setText( fileName );
    ui->lineEditVersion->setText( QString::number( m_Icon.getVersion() ) );
    ui->lineEditStackSize->setText( QString::number( m_Icon.getStackSize() ) );
    ui->comboBoxType->setCurrentIndex( m_Icon.getType() );
    ui->lineEditDefaultTool->setText( m_Icon.getDefaultTool() );
    ui->lineEditDepth->setText( QString::number( depth ) );
    ui->lineEditWidth->setText( QString::number( width ) );
    ui->lineEditHeight->setText( QString::number( height ) );

    //Now populate the tool types
    ui->plainTextEditToolTypes->clear();
    QVector<QString> toolTypes = m_Icon.getToolTypes();
    QVector<QString>::Iterator iter = toolTypes.begin();
    while( iter != toolTypes.end() )
    {
        QString nextToolType = (*iter);
        ui->plainTextEditToolTypes->appendPlainText( nextToolType );
        iter++;
    }

    //Show the 1st OS2 icon
    QImage iconImage = m_Icon.getOS2IconImage1();
    QPixmap iconPixMap;
    iconPixMap.convertFromImage( iconImage.scaledToHeight( 150 ) );
    ui->labelOS2Image1->setPixmap( iconPixMap );

    //Show the 2nd OS2 icon
    iconImage = m_Icon.getOS2IconImage2();
    iconPixMap.convertFromImage( iconImage.scaledToHeight( 150 ) );
    ui->labelOS2Image2->setPixmap( iconPixMap );

    //Show the OS35 icons
    QImage OS35Image1 = m_Icon.getOS35Image1();
    QImage OS35Image2 = m_Icon.getOS35Image2();
    QPixmap OS35Pixmap1;
    QPixmap OS35Pixmap2;
    OS35Pixmap1.convertFromImage( OS35Image1.scaledToHeight( 150 ) );
    OS35Pixmap2.convertFromImage( OS35Image2.scaledToHeight( 150 ) );
    ui->labelOS35Image1->setPixmap( OS35Pixmap1 );
    ui->labelOS35Image2->setPixmap( OS35Pixmap2 );

    //Now show the palette for debugging purposes
    int squareSize = 16;
    width = squareSize*squareSize;
    height = squareSize*squareSize;
    QImage paletteImage( QSize( width, height ), QImage::Format_ARGB32 );
    QPainter painter( &paletteImage );
    painter.begin( painter.device() );
    painter.setBrush( Qt::BrushStyle::SolidPattern );
    painter.fillRect( QRect( 0, 0, width, height ), OS35Image1.color( 0 ) );
    for( int c=0; c < OS35Image1.colorCount(); c++ )
    {
        //painter.setPen( OS35Image1.color( c ) );

        //Draw a rect with this colour
        int startX = c%squareSize*squareSize;
        int startY = (c/squareSize)*squareSize;
        painter.fillRect( QRect( startX, startY, squareSize, squareSize ), OS35Image1.color( c ) );
    }
    painter.end();

    QPixmap palettePixmap;
    palettePixmap.convertFromImage( paletteImage );
    ui->labelPalette->setPixmap( palettePixmap );
}

