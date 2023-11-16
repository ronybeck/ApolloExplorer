#include "diskvolume.h"
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
      m_Name( convertFromAmigaTextEncoding( entry.name ) )
{
    DBGLOG << "Volume Created: " << m_Name;
    m_SizeInBytes = (quint64)m_NumBlocks * (quint64)m_BytesPerBlock;
    m_BytesUsed = (quint64)m_NumBlocksUsed * (quint64)m_BytesPerBlock;
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
