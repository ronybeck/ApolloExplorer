#include "diskvolume.h"
#include <QPainter>
#include <QPainterPath>
#include <QtEndian>

#define DEBUG 0
#include "AEUtils.h"

DiskVolume::DiskVolume( VolumeEntry_t &entry, QObject *parent)
    : QObject{parent},
      m_NumSoftErrors( qFromBigEndian( entry.id_NumSoftErrors ) ),
      m_UnitNumber( qFromBigEndian( entry.id_UnitNumber ) ),
      m_DiskState( qFromBigEndian( entry.id_DiskState ) ),
      m_NumBlocks( qFromBigEndian( entry.id_NumBlocks ) ),
      m_NumBlocksUsed( qFromBigEndian( entry.id_NumBlocksUsed ) ),
      m_BytesPerBlock( qFromBigEndian( entry.id_BytesPerBlock ) ),
      m_DiskType( qFromBigEndian( entry.id_DiskType ) ),
      m_InUse( qFromBigEndian( entry.id_InUse ) ),
      m_Name( convertFromAmigaTextEncoding( entry.name ) ),
      m_PixMap( ":/browser/icons/Harddisk_Amiga.png" )
{
    DBGLOG << "Volume Created: " << m_Name;
    m_SizeInBytes = (quint64)m_NumBlocks * (quint64)m_BytesPerBlock;
    m_BytesUsed = (quint64)m_NumBlocksUsed * (quint64)m_BytesPerBlock;
    generatePixmap();
}

QString DiskVolume::getName()
{
    return m_Name;
}

quint64 DiskVolume::getSizeInBytes()
{
    return m_SizeInBytes;
}

quint64 DiskVolume::getUsedInBytes()
{
    return m_BytesUsed;
}

QPixmap DiskVolume::getPixmap()
{
    return m_PixMap;
}

void DiskVolume::setPixmap(QPixmap icon)
{
    m_PixMap = icon;
}

void DiskVolume::generatePixmap()
{
    QPainterPath path;

    //Setup the boundaries of our new image
    quint32 imageWidth = 85;
    quint32 imageHeight = 100;
    quint32 fontHeight = 12;
    quint32 barHeight = fontHeight + 4;

    //Basic icon
    QImage image( imageWidth, imageHeight, QImage::Format_ARGB32 );
    image.fill( 0x00000000 );
    QPainter painter( &image );

    //Setup up colours
    QPen barFillPen( Qt::darkGray );
    QPen barBoarderPen( 0xff181818 );
    QPen textPen( Qt::white );

    //Draw the disk icon image in our new image.
    QPixmap diskPixmap( ":/browser/icons/Harddisk_Amiga.png" );
    diskPixmap = diskPixmap.scaled( QSize( imageWidth, imageHeight - barHeight - 5 ), Qt::KeepAspectRatio,Qt::SmoothTransformation );
    if( !m_AmigaInfoFile.isNull() && m_AmigaInfoFile->hasImage() )
        diskPixmap = QPixmap::fromImage( m_AmigaInfoFile->getBestImage1().scaledToHeight( imageHeight - barHeight - 5, Qt::SmoothTransformation ) );
    //Center the image
    quint32 imagePositionLeft = (imageWidth - diskPixmap.width() ) / 2;
    painter.drawPixmap( imagePositionLeft, 0, diskPixmap );

    //Do the percentage full fill
    quint32 percentFull = 0;
    if( m_NumBlocksUsed > 0 )
    {
        percentFull = (quint64)m_NumBlocksUsed * (qint64)imageWidth / (quint64)m_NumBlocks;
        path.addRect( QRect( 0, imageHeight - barHeight - 1, percentFull, barHeight ) );
        painter.setPen( barFillPen );
        painter.fillPath( path, barFillPen.color() );
    }

    //Write the percent full text
    painter.setPen( textPen );
    painter.setFont( QFont(":/comfortaa", fontHeight, QFont::DemiBold) );
    painter.drawText( imageWidth/3, imageHeight - 3, QString::number( percentFull ) + "%" );

    //Draw the border
    painter.setPen( barBoarderPen );
    painter.drawRect( 0, imageHeight - barHeight - 1, imageWidth - 1, barHeight );

    //Set our new image as our icon image
    m_PixMap = QPixmap::fromImage( image );
}

QSharedPointer<AmigaInfoFile> DiskVolume::getAmigaInfoFile() const
{
    return m_AmigaInfoFile;
}

void DiskVolume::setAmigaInfoFile( QSharedPointer<AmigaInfoFile> newAmigaInfoFile )
{
    m_AmigaInfoFile = newAmigaInfoFile;

    //We should get the icon out of the info file and set that as our own
    //Get the best image we can for the icon
    if( !m_AmigaInfoFile->hasImage() )
    {
        //m_PixMap = QPixmap::fromImage( m_AmigaInfoFile->getBestImage1() );
        m_AmigaInfoFile = nullptr;
    }

    generatePixmap();
}
