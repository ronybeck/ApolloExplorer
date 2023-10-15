#include "bitgetter.h"

BitGetter::BitGetter( QByteArray bytes ) :
    m_Bytes( bytes ),
    m_CurrentChunk( 0 ),
    m_BitsLeftInChunk( 0 )
{

}

bool BitGetter::getBits( quint8 numberOfBits, quint8 &buffer )
{
    quint8 *nextBytes = reinterpret_cast<quint8*>( &m_CurrentChunk );

    //Sanity check
    if( numberOfBits > 8 )  return false;
    if( m_Bytes.size() == 0 ) return false;

    //Do we have zero bytes left in the chunk?
    if( m_BitsLeftInChunk == 0 && m_Bytes.size() > 0 )
    {
        //Get the next byte
        nextBytes[ 1 ] = m_Bytes.constData()[ 0 ];
        m_Bytes.remove( 0, 1 );
        m_BitsLeftInChunk = 8;
    }

    //Can we get exactly one more byte into our chunk?
    if( m_BitsLeftInChunk == 8 && m_Bytes.size() > 0 )
    {
        nextBytes[ 0 ] = m_Bytes.constData()[ 0 ];
        m_Bytes.remove( 0, 1 );
        m_BitsLeftInChunk += 8;
    }

    //Do we have less than 8 bits in our chunk?
    if( m_BitsLeftInChunk < 8 && m_BitsLeftInChunk > 0 && m_Bytes.size() > 0 )
    {
        //First right shift the remaining bits to the right 8bit boundary
        m_CurrentChunk = m_CurrentChunk >> ( 8 - m_BitsLeftInChunk );

        //Get the next byte int
        nextBytes[ 0 ] = m_Bytes.constData()[ 0 ];
        m_Bytes.remove( 0, 1 );

        //Now shift the bits left again so they are at their original position
        m_CurrentChunk = m_CurrentChunk << ( 8 - m_BitsLeftInChunk );
        m_BitsLeftInChunk += 8;
    }

    //Finally extract the bits needed
    buffer = nextBytes[ 1 ];

    //shift the requested bits in the return buffer into position
    buffer = buffer >> ( 8 - numberOfBits );

    //Shift our chunk bits
    m_CurrentChunk = m_CurrentChunk << numberOfBits;
    m_BitsLeftInChunk -= numberOfBits;

    return true;
}

QByteArray BitGetter::getBitStream(quint64 numberOfChunks, quint8 bitsPerChunk)
{
    quint8 buffer = 0;
    QByteArray chunks;

    //just loop until we have all the chunks
    for( quint64 chunk = 0; chunk < numberOfChunks; chunk++ )
    {
        //Get what we can
        if( getBits( bitsPerChunk, buffer ) != true )
            return chunks;

        //Add this to our chunks list
        chunks.push_back( buffer );
    }

    return chunks;
}
