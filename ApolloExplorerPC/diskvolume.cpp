#include "diskvolume.h"
#include <QPainter>
#include <QPainterPath>
#include <QtEndian>

#define DEBUG 1
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
      m_Name( entry.name ),
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

void DiskVolume::generatePixmap()
{
    //Basic icon
    QImage image( 100, 85, QImage::Format_ARGB32 );
    image.fill( 0x00000000 );
    QPainter painter( &image );
    QPixmap diskImage( ":/browser/icons/Harddisk_Amiga.png" );

    //Now the bar indicating disk usage
    quint32 percentFull = 0;
    if( m_NumBlocksUsed > 0 ) percentFull = m_NumBlocksUsed * 100 / m_NumBlocks;
    painter.drawPixmap( 6,0, diskImage );
    QPainterPath path;
    QPen redPen( Qt::red );
    QPen blackPen( Qt::black );
    path.addRect( QRect( 0, 60, percentFull, 20 ) );
    painter.setPen( redPen );
    painter.fillPath( path, Qt::red );
    painter.setPen( blackPen );
    painter.drawRect( 0, 60, 99, 20 );
    //painter.drawText( 5, 75, prettyFileSize( getSizeInBytes(), false ) + "/" + prettyFileSize( getUsedInBytes() ) );
    painter.drawText( 35, 75, QString::number( percentFull ) + "%" );
    m_PixMap = QPixmap::fromImage( image );
}
