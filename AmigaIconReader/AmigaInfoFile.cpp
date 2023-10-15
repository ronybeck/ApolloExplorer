#include "AmigaInfoFile.h"

#include <iostream>
#include <bitset>
#include <QDebug>
#include <QFile>
#include <QtEndian>
#include <QColor>

#include "bitgetter.h"


static inline quint32 getULONGFromBuffer( QByteArray someBuffer, quint64 &someOffset )
{
    quint32 val;
    memcpy( &val, someBuffer.constData() + someOffset, 4 );
    val = qFromBigEndian( val );
    someOffset+=4;
    return val;
}

static inline quint16 getUWORDFromBuffer( QByteArray someBuffer, quint64 &someOffset )
{
    quint16 val;
    memcpy( &val, someBuffer.constData() + someOffset, 2 );
    val = qFromBigEndian( val );
    someOffset+=2;
    return val;
}


static inline quint8 getUBYTEFromBuffer( QByteArray someBuffer, quint64 &someOffset )
{
    quint8 val;
    memcpy( &val, someBuffer.constData() + someOffset, 1 );
    someOffset+=1;
    return val;
}


static inline void getStringFromBuffer( QByteArray someBuffer, quint64 &someOffset, char *dstBuffer, const quint64 dstBufferSize )
{
    memcpy( dstBuffer, someBuffer.constData() + someOffset, dstBufferSize );
    someOffset+=dstBufferSize;
}


static inline QByteArray extractByteArray( QByteArray someBuffer, quint64 &someOffset, const quint64 numberOfBytes )
{
    QByteArray extractedData( someBuffer.constData() + someOffset, numberOfBytes );
    someOffset+=numberOfBytes;
    return extractedData;
}


static inline void putUBYTEInBuffer( QByteArray &someBuffer, quint64 &someOffset, const quint8 byte )
{
    someBuffer.append( byte );
    someOffset++;
}

static inline void putByteArrayInBuffer( QByteArray &someBuffer, quint64 &someOffset, const QByteArray sourceBytes )
{
    someBuffer.append( sourceBytes );
    someOffset += sourceBytes.size();
}


static inline QByteArray BitPlanarToChunky( QByteArray bitPlanarPixels, quint32 width, quint32 height, quint8 bitsPerPixel )
{
    //Set aside some memory for our chunky icon
    char indexedImageData[ width * height ];
    memset( indexedImageData, 0, width * height );

    //Load in the bitplanar bitmap
    quint64 pitplaneRowByteLength = ((width+15)/16)*2;
    quint64 bitplaneSizeInBytes = pitplaneRowByteLength * height;

    //Decode bitplanar into indexed colour mapped pixels
    for( quint8 bitPlane = 0; bitPlane < bitsPerPixel; bitPlane++ )
    {
        for( quint32 y=0; y < height; y++ )
        {
            for( quint32 x=0; x < width; x++ )
            {
                quint8 bit = 7-x%8;
                quint8 srcPixelByte = bitPlanarPixels.constData()[ (bitPlane * bitplaneSizeInBytes ) + ( y*pitplaneRowByteLength ) + ( x/8 ) ];
                srcPixelByte = qFromBigEndian( srcPixelByte );
                quint8 dstPixelByte = indexedImageData[ x+(y*width) ];
                quint8 bitComparator = (1<<bit);
                quint8 resultingPixelByte = (srcPixelByte & bitComparator ? (1<<bitPlane) : 0 ) | dstPixelByte;
                indexedImageData[ x+(y*width) ] = resultingPixelByte;
            }
        }
    }

    return QByteArray( indexedImageData, width * height );
}


AmigaInfoFile::AmigaInfoFile() :
    m_Icon(),
    m_DrawerData(),
    m_DrawerData2(),
    m_Image1(),
    m_Image2(),
    m_OS2Image1(),
    m_OS2Image2(),
    m_OS35Image1(),
    m_OS35Image2(),
    m_DefaultTool(),
    m_ToolTypes(),
    m_Priority( 0 ),    //Note: default priority depends on the OS
    m_ToolWindow( )
{
    QObject();

    //This is just as good as initialising
    reset();
}

bool AmigaInfoFile::loadFile(QString path)
{
    QFile file( path );

    //Clear out some stuff
    reset();

    //Open the file
    if( !file.open( QFile::ReadOnly ) )
    {
        qDebug() << "Failed to open " << path;
        return false;
    }

    //Get the file size and allocate memory for the file
    quint64 fileSize = file.size();

    //Now read in the file
    QByteArray buffer = file.readAll();
    file.close();
    if( static_cast<quint64>(buffer.size()) !=  fileSize )
    {
        qDebug() << "Unexpected file read size of " << buffer.size() << ".  Expected " << fileSize << ".";
        return false;
    }

    //Process the contetns of the file
    return process( buffer );
}

static bool isPNGIcon( QByteArray data )
{
    //PNG Files start with hex code 89 followed by 'PNG'
    if( data.constData()[ 1 ] != 'P' )  return false;
    if( data.constData()[ 2 ] != 'N' )  return false;
    if( data.constData()[ 3 ] != 'G' )  return false;
    return true;
}

bool AmigaInfoFile::process(QByteArray data)
{
    //First, just double check this isn't a PNG file
    if( isPNGIcon( data ) )
    {
        //Ok so we have just a raw PNG file.  Let's load that
        if( !m_OS35Image1.loadFromData( data ) )
        {
            qDebug() << "Failed to load the icon as a PNG despite there being a PNG header.";
            return false;
        }

        //Copy this to the other image
        m_OS35Image2 = m_OS35Image1;

        return true;
    }

    //now copy the byte array to our icon storage
    memcpy( &m_Icon, data.constData(), sizeof( m_Icon ) );

    //Now we have to do some byte swapping
    m_Icon.do_Magic = qFromBigEndian( m_Icon.do_Magic );
    m_Icon.do_Version = qFromBigEndian( m_Icon.do_Version );
    m_Icon.do_Type = qFromBigEndian( m_Icon.do_Type );
    m_Icon.do_DrawerData = qFromBigEndian( m_Icon.do_DrawerData );
    m_Icon.do_CurrentX = qFromBigEndian( m_Icon.do_CurrentX );
    m_Icon.do_CurrentY = qFromBigEndian( m_Icon.do_CurrentY );
    m_Icon.do_StackSize = qFromBigEndian( m_Icon.do_StackSize );
    m_Icon.do_Gadget.ga_Flags = qFromBigEndian( m_Icon.do_Gadget.ga_Flags );
    m_Icon.do_Gadget.ga_Width = qFromBigEndian( m_Icon.do_Gadget.ga_Width );
    m_Icon.do_Gadget.ga_Height = qFromBigEndian( m_Icon.do_Gadget.ga_Height );
    m_Icon.do_Gadget.ga_TopEdge = qFromBigEndian( m_Icon.do_Gadget.ga_TopEdge );
    m_Icon.do_Gadget.ga_LeftEdge = qFromBigEndian( m_Icon.do_Gadget.ga_LeftEdge );
    m_Icon.do_Gadget.ga_UserData = qFromBigEndian( m_Icon.do_Gadget.ga_UserData & 0xff );
    m_Icon.do_Gadget.ga_SelectRender = qFromBigEndian( m_Icon.do_Gadget.ga_SelectRender ) & 0xff;


    //Now we need to extract the trailing data
    quint64 offset = sizeof( m_Icon );

    //Do we have Drawe Data?
    if( m_Icon.do_DrawerData != 0 )
    {
        //We have draw data
        memcpy( &m_DrawerData, data.constData() + offset, sizeof( DrawerData_t ) );

        //Now we have to do some endian conversion
        m_DrawerData.dd_CurrentX = qFromBigEndian( m_DrawerData.dd_CurrentX );
        m_DrawerData.dd_CurrentY = qFromBigEndian( m_DrawerData.dd_CurrentY );

        //Shift our offset
        offset += sizeof( DrawerData_t );
    }

    //Ok Next we should have the image data
    memcpy( &m_Image1, data.constData() + offset, sizeof( m_Image1 ) );

    //Now we have to do some endian conversion
    m_Image1.Depth = qFromBigEndian( m_Image1.Depth );
    m_Image1.Height = qFromBigEndian( m_Image1.Height );
    m_Image1.Width = qFromBigEndian( m_Image1.Width );
    m_Image1.LeftEdge = qFromBigEndian( m_Image1.LeftEdge );
    m_Image1.TopEdge = qFromBigEndian( m_Image1.TopEdge );

    //Sanity check
    if( m_Image1.Width > 200 || m_Image1.Width < 0 ||
        m_Image1.Height > 200 || m_Image1.Height < 0 ||
        m_Image1.Depth > 8  || m_Image1.Depth < 0 )
    {
        qDebug() << "Something is not right with the first OS2 icon.  The dimentions are: ";
        qDebug() << "Width: " << m_Image1.Width;
        qDebug() << "Height: " << m_Image1.Height;
        qDebug() << "Depth: " << m_Image1.Depth;
        //return false;
    }else
    {
        //Adjust the offset
        offset += sizeof( Image_t );

        //Decode the icon planes
        QByteArray indexedImageData = decodeOS2Icon( data, offset, m_Image1.Depth, m_Image1.Width, m_Image1.Height );

        //Draw the icon
        m_OS2Image1 = drawIndexedOS2Icon( indexedImageData, m_Image1.Width, m_Image1.Height );

        //Automatically copy this to the second image.  It can be over written later if there is one
        m_OS2Image2 = m_OS2Image1;

        //Now we should get the second image if there is one
        if( m_Icon.do_Gadget.ga_SelectRender != 0 || m_Icon.do_Gadget.ga_Flags & 2 )
        {
            memcpy( &m_Image2, data.constData() + offset, sizeof( m_Image2 ) );

            //Now we have to do some endian conversion
            m_Image2.Depth = qFromBigEndian( m_Image2.Depth );
            m_Image2.Height = qFromBigEndian( m_Image2.Height );
            m_Image2.Width = qFromBigEndian( m_Image2.Width );
            m_Image2.LeftEdge = qFromBigEndian( m_Image2.LeftEdge );
            m_Image2.TopEdge = qFromBigEndian( m_Image2.TopEdge );

            //Sanity check
            if( m_Image2.Width > 200 || m_Image2.Width < 0 ||
                m_Image2.Height > 200 || m_Image2.Height < 0 ||
                m_Image2.Depth > 8  || m_Image2.Depth < 0 )
            {
                qDebug() << "Something is not right with the second OS2 icon.  The dimentions are: ";
                qDebug() << "Width: " << m_Image2.Width;
                qDebug() << "Height: " << m_Image2.Height;
                qDebug() << "Depth: " << m_Image2.Depth;
                //return false;
            }else
            {
                //Adjust the offset
                offset += sizeof( Image_t );

                //Decode the icon planes
                indexedImageData = decodeOS2Icon( data, offset, m_Image2.Depth, m_Image2.Width, m_Image2.Height );

                //Draw the icon
                m_OS2Image2 = drawIndexedOS2Icon( indexedImageData, m_Image2.Width, m_Image2.Height );
            }
        }
    }

    //Now we should get the texts out
    //Do we have a Default Tool Text?
    if( m_Icon.do_DefaultTool != 0 )
    {
        //Get the text length
        quint32 length;
        memcpy( &length, data.constData() + offset, 4 );
        length = qFromBigEndian( length );
        offset+=4;

        //Now get the default text
        QByteArray defaultTool( data.constData() + offset, length );
        m_DefaultTool = defaultTool.constData();

        //Adjust the offset
        offset += length;
    }

    //Now we process the tool types
    if( m_Icon.do_ToolTypes != 0 )
    {
        //Get the number of Entries
        quint32 length;
        memcpy( &length, data.constData() + offset, 4 );
        length = qFromBigEndian( length );

        //Now to make things interesting, the length is the entry count increased by 1 and multipled by four
        quint8 entryCount = length/4 -1;

        offset+=4;

        for( quint8 i=0; i < entryCount; i++ )
        {
            //Get the size of the next entry
            memcpy( &length, data.constData() + offset, 4 );
            length = qFromBigEndian( length );
            offset+=4;

            //Now get the default text
            QByteArray toolType( data.constData() + offset, length );
            QString nextToolType = toolType.constData();
            m_ToolTypes.push_back( nextToolType );

            //Special case:  The priority of the task is a tool type!
            if( nextToolType.startsWith( "STARTPRI" ) || nextToolType.startsWith( "TOOLPRI" ) )
            {
                auto components = nextToolType.split( "=" );
                if( components.size() > 1 )
                {
                    QString priority = components[ 1 ];
                    m_Priority = priority.toInt();
                }

            }

            //Adjust the offset
            offset += length;
        }
    }

    //Do we have a Tool Window text
    if( m_Icon.do_ToolWindow != 0 )
    {
        //Get the text length
        quint32 length;
        memcpy( &length, data.constData() + offset, 4 );
        length = qFromBigEndian( length );
        offset+=4;

        //Now get the default text
        QByteArray toolWindow( data.constData() + offset, length );
        m_ToolWindow = toolWindow.constData();

        //Adjust the offset
        offset += length;
    }

    //Do we have some DrawerData2?
    if( m_Icon.do_DrawerData != 0 /*&& m_Icon.do_Gadget.ga_UserData & 1*/ )
    {
        memcpy( &m_DrawerData2, data.constData() + offset, sizeof( m_DrawerData2 ) );
        offset += sizeof( m_DrawerData2 );

        m_DrawerData2.dd_Flags = qFromBigEndian( m_DrawerData2.dd_Flags );
        m_DrawerData2.dd_ViewModes = qFromBigEndian( m_DrawerData2.dd_ViewModes );
    }

    //Do we have an OS3.5 icon?
    if( data.size() > static_cast<int>( offset+16 ) )
    {
        char formChars[ 5 ] = { 0, 0, 0, 0, 0 };
        char iconChars[ 5 ] = { 0, 0, 0, 0, 0 };
        quint32 length;

        //Get the "FORM" text
        memcpy( formChars, data.constData() + offset, 4 );
        offset+=4;

        //Get the length
        memcpy( &length, data.constData() + offset, 4 );
        length = qFromBigEndian( length );
        offset+=4;

        //Get the "ICON" text
        memcpy( iconChars, data.constData() + offset, 4 );
        offset+=4;

        //Check if we have the header for the first image
        if( !strcmp( formChars, "FORM" ) && !strcmp( iconChars, "ICON" ) )
        {
            //Yep, we have an OS35 icon
            QByteArray imageData;
            QByteArray paletteData;
            quint32 width;
            quint32 height;
            quint8 numberOfColours;
            qint16 transparentColour;
            decodeOS35Icon( data, offset, imageData, paletteData, width, height, numberOfColours, transparentColour  );
            //Sanity Check:  Sometimes the decoding goes wrong and we get insane dimensions
            if( width < 500 && height < 500 )
            {
                m_OS35Image1 = drawIndexedOS35Icon( imageData, paletteData, numberOfColours, width, height, transparentColour );

                //Automatically copy the first image to the second in case there isn't a second image
                m_OS35Image2 = m_OS35Image1;
            }
        }

        //Check if we have the header for the second image
        if( data.size() > static_cast<int>( offset+16 ) )
        {
            //Get the "FORM" text
            memcpy( formChars, data.constData() + offset, 4 );
            offset+=4;

            //Get the length
            memcpy( &length, data.constData() + offset, 4 );
            length = qFromBigEndian( length );
            offset+=4;

            //Get the "ICON" text
            memcpy( iconChars, data.constData() + offset, 4 );
            offset+=4;
            if( !strcmp( formChars, "FORM" ) && !strcmp( iconChars, "ICON" ) )
            {
                //Yep, we have an OS35 icon
                QByteArray imageData;
                QByteArray paletteData;
                quint32 width;
                quint32 height;
                quint8 numberOfColours;
                qint16 transparentColour;
                decodeOS35Icon( data, offset, imageData, paletteData, width, height, numberOfColours, transparentColour );
                //Sanity check.  Sometimes the decoding goes wrong and we get insane dimensions
                if( width < 500 && height < 500 )
                {
                    m_OS35Image2 = drawIndexedOS35Icon( imageData, paletteData, numberOfColours, width, height, transparentColour );
                }
            }
        }
    }

    return true;
}

IconType_t AmigaInfoFile::getType()
{
    return static_cast<IconType_t>( m_Icon.do_Type );
}

void AmigaInfoFile::setType(IconType_t type)
{
    if( type >= DISK && type <= APPICON )
    {
        m_Icon.do_Type = type;
    }
}

quint16 AmigaInfoFile::getVersion()
{
    return m_Icon.do_Version;
}

void AmigaInfoFile::setVersion(quint16 version)
{
    m_Icon.do_Version = version;
}

quint32 AmigaInfoFile::getStackSize()
{
    return m_Icon.do_StackSize;
}

void AmigaInfoFile::setStackSize(quint32 size)
{
    m_Icon.do_StackSize = size;
}

QString AmigaInfoFile::getDefaultTool()
{
    return m_DefaultTool;
}

void AmigaInfoFile::setDefaultTool(QString text)
{
    m_DefaultTool = text;
}

QVector<QString> AmigaInfoFile::getToolTypes()
{
    return m_ToolTypes;
}

void AmigaInfoFile::getOS2IconParameters(quint8 &depth, quint32 &width, quint32 &height)
{
    depth = m_Image1.Depth;
    width = m_Image1.Width;
    height = m_Image1.Height;
}

qint16 AmigaInfoFile::getPriority()
{
    return m_Priority;
}

QByteArray AmigaInfoFile::decodeOS2Icon(QByteArray data, quint64 &offset, quint8 depth, quint32 width, quint32 height )
{
    //Set aside some memory for our chunky icon
    char indexedImageData[ width * height ];
    memset( indexedImageData, 0, width * height );

    //Load in the bitplanar bitmap
    quint64 pitplaneRowByteLength = ((width+15)/16)*2;
    quint64 bitplaneSizeInBytes = pitplaneRowByteLength * height;
    QByteArray bitPlanes( data.constData() + offset, bitplaneSizeInBytes * depth );

    //Decode bitplanar into indexed colour mapped pixels
    for( quint8 bitPlane = 0; bitPlane < depth; bitPlane++ )
    {
        for( quint32 y=0; y < height; y++ )
        {
            for( quint32 x=0; x < width; x++ )
            {
                quint8 bit = 7-x%8;
                quint8 srcPixelByte = bitPlanes.constData()[ (bitPlane * bitplaneSizeInBytes ) + ( y*pitplaneRowByteLength ) + ( x/8 ) ];
                srcPixelByte = qFromBigEndian( srcPixelByte );
                quint8 dstPixelByte = indexedImageData[ x+(y*width) ];
                quint8 bitComparator = (1<<bit);
                quint8 resultingPixelByte = (srcPixelByte & bitComparator ? (1<<bitPlane) : 0 ) | dstPixelByte;
                indexedImageData[ x+(y*width) ] = resultingPixelByte;
            }
        }
    }

    //Adjust the offset
    offset += bitplaneSizeInBytes * depth;

    return QByteArray( indexedImageData, width * height );
}





void AmigaInfoFile::decodeOS35Icon(QByteArray data, quint64 &offset, QByteArray &image, QByteArray &palette, quint32 &imageWidth, quint32 &imageHeight, quint8 &colourCount, qint16 &transparentColour )
{
    QByteArray decodedImageBytes;
    quint8 width = 0;
    quint8 height = 0;
    quint8 flags = 0;
    quint8 xAspect = 0;
    quint8 yAspect = 0;
    quint8 numberOfColours = 0;
    quint16 paletteSize = 0;
    quint16 imageByteSize;
    quint8 imageFormat = 0;
    quint8 paletteFormat = 0;
    quint8 bitsPerPixel = 0;
    quint8 imageFlags = 0;
    char faceChars[ 5 ] = { 0, 0, 0, 0, 0 };
    char imageChars[ 5 ] = { 0, 0, 0, 0, 0 };
    quint32 length = 0;
    Q_UNUSED( flags )
    Q_UNUSED( xAspect )
    Q_UNUSED( yAspect )
    Q_UNUSED( length )

    //First get the "face" header out
    getStringFromBuffer( data, offset, faceChars, 4 );
    if( strcmp( faceChars, "FACE" ) )
    {
        //We couldn't find the face header
        qDebug() << "We couldn't find the \"FACE\" header";
        return;
    }

    //Get the face data
    length = getULONGFromBuffer( data, offset );
    width = getUBYTEFromBuffer( data, offset ) + 1;
    height = getUBYTEFromBuffer( data, offset ) + 1;
    flags = getUBYTEFromBuffer( data, offset );
    quint8 aspect = getUBYTEFromBuffer( data, offset );
    xAspect = aspect>>4;
    yAspect = aspect&0x0F;
    paletteSize = getUWORDFromBuffer( data, offset );

    //Now we want to get the next chunk which should be an "IMAG"
    getStringFromBuffer( data, offset, imageChars, 4 );
    if( strcmp( imageChars, "IMAG" ) )
    {
        //We couldn't find the IMAG header
        qDebug() << "We couldn't find the \"IMAG\" header";
        return;
    }

    //Now get the image parameters
    length = getULONGFromBuffer( data, offset );
    transparentColour = getUBYTEFromBuffer( data, offset );
    numberOfColours = getUBYTEFromBuffer( data, offset ) + 1;
    imageFlags = getUBYTEFromBuffer( data, offset );
    imageFormat = getUBYTEFromBuffer( data, offset );
    paletteFormat = getUBYTEFromBuffer( data, offset );
    bitsPerPixel = getUBYTEFromBuffer( data, offset );
    imageByteSize = getUWORDFromBuffer( data, offset ) + 1;
    paletteSize = getUWORDFromBuffer( data, offset ) + 1;

    //Do we really have a transparent colour?  If not, set this to -1
    if( !( imageFlags & 0x01 ) )
    {
        transparentColour = -1;
    }

    //This is our image storage
    QByteArray imageData = extractByteArray( data, offset, imageByteSize );
    QByteArray paletteData = extractByteArray( data, offset, paletteSize );

    //IF we have compressed image data, do the decompression now
    if( imageFormat == 1 )
    {
        quint64 uncompressedImageDataOffset = 0;
        QByteArray uncompressedData;
        quint8 nextByte;
        quint8 nextPixel;
        BitGetter bitGetter( imageData );
        quint64 uncompressedImageSize = width * height;

        while( uncompressedImageDataOffset < uncompressedImageSize  )
        {
            //Get the next RLE chunk
            if( bitGetter.getBits( 8, nextByte ) != true )
                break;

            //Process the chunk
            if( nextByte <= 127 )
            {
                //Copy the next X bytes
                quint8 copyLength = nextByte + 1;

                //get the bytes from the compressed stream
                QByteArray nextPixels = bitGetter.getBitStream( copyLength, bitsPerPixel );

                //Add these pixels to the uncompressed array
                putByteArrayInBuffer( uncompressedData, uncompressedImageDataOffset, nextPixels );

            } else if( nextByte >= 129 )
            {
                //Copy the next byte X times
                quint8 copyCount = 257 - nextByte;
                if( bitGetter.getBits( bitsPerPixel, nextPixel ) != true )
                    break;

                //Now do the copy
                for( int i = 0; i < copyCount; i++ )
                {
                    putUBYTEInBuffer( uncompressedData, uncompressedImageDataOffset, nextPixel );
                }
            }
        }

        //Now do the bitplanar to chunky conversion
        imageData.clear();
        //imageData.append( BitPlanarToChunky( uncompressedData, width, height, bitsPerPixel ) );
        imageData.append( uncompressedData );
    }

    //Do we have a palette attached?
    if( imageFlags & (1<<1) )
    {
        //If we have a compressed palette, we need to decompress it
        if( paletteFormat == 1 )
        {
            quint64 compressedPaletteDataOffset = 0;
            quint64 uncompressedPaletteDataOffset = 0;
            QByteArray uncompressedData;

            while( uncompressedPaletteDataOffset < paletteSize  )
            {
                //Get the next RLE chunk
                quint8 nextByte = 0;
                nextByte = getUBYTEFromBuffer( paletteData, compressedPaletteDataOffset );

                //Process the chunk
                if( nextByte <= 127 )
                {
                    //Copy the next X bytes
                    quint8 copyLength = nextByte + 1;

                    //get the bytes from the compressed stream
                    QByteArray nextBytes = extractByteArray( paletteData, compressedPaletteDataOffset, copyLength );

                    //Add these pixels to the uncompressed array
                    putByteArrayInBuffer( uncompressedData, uncompressedPaletteDataOffset, nextBytes );

                } else if( nextByte >= 129 )
                {
                    //Copy the next byte X times
                    quint8 copyCount = 257 - nextByte;
                    nextByte = getUBYTEFromBuffer( paletteData, compressedPaletteDataOffset );

                    //Now do the copy
                    for( int i = 0; i < copyCount; i++ )
                    {
                        putUBYTEInBuffer( uncompressedData, uncompressedPaletteDataOffset, nextByte );
                    }
                }
            }
            //Now replace the original Image data with our uncompressed version
            paletteData.clear();
            paletteData.append( uncompressedData );
        }
    }

    //Now we have all the data we need to give back to the caller
    image.clear();
    image.append( imageData );
    palette.clear();
    palette.append( paletteData );
    imageWidth = width;
    imageHeight = height;
    colourCount = numberOfColours;

    return;
}

QImage AmigaInfoFile::drawIndexedOS2Icon( QByteArray data, quint32 width, quint32 height )
{
    QImage theImage = QImage( width, height, QImage::Format_Indexed8 );
    theImage.setColor( 0, 0xFF888888 );
    theImage.setColor( 1, 0xFF000000 );
    theImage.setColor( 2, 0xFFFFFFFF );
    theImage.setColor( 3, 0xFF0000FF );
    theImage.setColor( 4, 0xFFFF0000 );
    theImage.setColor( 5, 0xFFAAAAAA );
    theImage.setColor( 6, 0xFF777777 );
    theImage.setColor( 7, 0xFF444444 );
    theImage.setColor( 8, 0xFF222222 );


    //Draw the image
    for( quint32 y = 0; y < height; y++ )
    {
        for( quint32 x = 0; x < width; x++ )
        {
            theImage.setPixel( x, y, static_cast<quint8>( data.constData()[ x + y*width ] ) );
        }
    }

    return theImage;
}

QImage AmigaInfoFile::drawIndexedOS35Icon(QByteArray imageData, QByteArray paletteData, quint8 numberOfColours, quint32 width, quint32 height, qint16 transparentColour )
{
    //First, create our image
    QImage theImage = QImage( width, height, QImage::Format_ARGB32 );

    //Create an index of all the colours
    QList<QColor> colourPalette;

    //Now setup the palette
    quint64 paletteOffset = 0;
    for( quint8 i = 0; i < numberOfColours; i++ )
    {
        //Extract the colour components
        quint8 red = getUBYTEFromBuffer( paletteData, paletteOffset );
        quint8 green = getUBYTEFromBuffer( paletteData, paletteOffset );
        quint8 blue = getUBYTEFromBuffer( paletteData, paletteOffset );
        quint8 alpha = 255;
        if( i == transparentColour )    alpha = 0;

        //Create the colour
        QColor pixelColour( red, green, blue,  alpha );
        //theImage.setColor( i, pixelColour.rgb() );
        colourPalette.push_back( pixelColour );

        //qDebug() << "Colour " << i << " is defined as " << pixelColour;
    }

    //Now draw the pixels
    quint64 imageOffset = 0;
    for( quint32 y = 0; y < height; y++ )
    {
        for( quint32 x = 0; x < width; x++ )
        {
            quint8 nextPixel = getUBYTEFromBuffer( imageData, imageOffset );
            if( nextPixel >= numberOfColours )
            {
                qDebug() << "Pixel " << nextPixel << " is higher than the colour count";
                nextPixel = 0;  //Set this to the colour 0 as a work around.
            }
            //theImage.setPixel( x, y, nextPixel );
            theImage.setPixelColor( x, y, colourPalette[ nextPixel ] );
        }
        std::cout << std::endl;
    }

    return theImage;
}

QImage AmigaInfoFile::getOS2IconImage1()
{
    return m_OS2Image1;
}

QImage AmigaInfoFile::getOS2IconImage2()
{
    return m_OS2Image2;
}

QImage AmigaInfoFile::getOS35Image1()
{
    return m_OS35Image1;
}

QImage AmigaInfoFile::getOS35Image2()
{
    return m_OS35Image2;
}

QImage AmigaInfoFile::getBestImage1()
{
    //Do we have an OS35 image?
    if( !m_OS35Image1.isNull() )    return m_OS35Image1;
    if( !m_OS2Image1.isNull() )     return m_OS2Image1;

    //Otherwise return an emptyimage;
    return QImage();
}

QImage AmigaInfoFile::getBestImage2()
{
    //Do we have an OS35 image?
    if( !m_OS35Image2.isNull() )    return m_OS35Image2;
    if( !m_OS35Image1.isNull() )    return m_OS35Image1;
    if( !m_OS2Image2.isNull() )     return m_OS2Image2;
    if( !m_OS2Image1.isNull() )     return m_OS2Image1;

    //Otherwise return an emptyimage;
    return QImage();
}



void AmigaInfoFile::reset()
{
    memset( &m_Icon, 0, sizeof( m_Icon) );
    memset( &m_DrawerData, 0, sizeof( m_DrawerData ) );
    memset( &m_DrawerData2, 0, sizeof( m_DrawerData2 ) );
    memset( &m_Image1, 0, sizeof( m_Image1 ) );
    memset( &m_Image2, 0, sizeof( m_Image2 ) );
    m_OS2Image1 = QImage();
    m_OS2Image2 = QImage();
    m_OS35Image1 = QImage();
    m_OS35Image2 = QImage();
    m_DefaultTool = "";
    m_ToolTypes.clear();
    m_ToolWindow = "";
}

