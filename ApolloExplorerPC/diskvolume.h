#ifndef DISKVOLUME_H
#define DISKVOLUME_H

#include <QObject>
#include <QPixmap>
#include <QSharedPointer>

#include "protocolTypes.h"
#include "../AmigaIconReader/AmigaInfoFile.h"

class DiskVolume : public QObject
{
    Q_OBJECT
public:
    explicit DiskVolume( VolumeEntry_t &entry, QObject *parent = nullptr);

    QString getName();
    quint64 getSizeInBytes();
    quint64 getUsedInBytes();
    QPixmap getPixmap();
    QString getFileSystemType();
    void setPixmap( QPixmap icon );

    QSharedPointer<AmigaInfoFile> getAmigaInfoFile() const;
    void setAmigaInfoFile( QSharedPointer<AmigaInfoFile> newAmigaInfoFile );

private:
    void generatePixmap();

signals:

private:
    int m_NumSoftErrors;
    int m_UnitNumber;
    int m_DiskState;
    int m_NumBlocks;
    int m_NumBlocksUsed;
    int m_BytesPerBlock;
    int m_DiskType;
    int m_InUse;
    QString m_FileSystemDesignation;
    quint64 m_BytesUsed;
    quint64 m_SizeInBytes;
    QString m_Name;
    QPixmap m_PixMap;
    QSharedPointer<AmigaInfoFile> m_AmigaInfoFile;
};

#endif // DISKVOLUME_H
