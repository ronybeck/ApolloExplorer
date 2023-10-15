#ifndef BITGETTER_H
#define BITGETTER_H

#include <QByteArray>

class BitGetter
{
public:
    BitGetter( QByteArray bytes );

    bool getBits( quint8 numberOfBits, quint8 &buffer );
    QByteArray getBitStream( quint64 numberOfChunks, quint8 bitsPerChunk );

private:
    QByteArray m_Bytes;
    quint16 m_CurrentChunk;
    quint8 m_BitsLeftInChunk;
};

#endif // BITGETTER_H
