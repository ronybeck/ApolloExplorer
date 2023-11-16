#include "AEUtils.h"

#include <QTextEncoder>
#include <QDebug>

QString prettyFileSize( quint64 size, bool withUnits )
{
#define GigaByte (1024*1024*1024)
#define MegaByte (1024*1024)
#define KiloByte 1024
    if( withUnits )
    {
        if( size > GigaByte )
        {
//            quint64 gigaBytes = size/GigaByte;
//            quint64 remainder = size%GigaByte;
//            quint64 remainderAsGB = remainder/MegaByte/100;
//            return QString( QString::number( gigaBytes, 10 ) + "." + QString::number( remainderAsGB ) + "GB" );
            return QString( QString::number( size / GigaByte, 10 ) + "." + QString::number( size%GigaByte/MegaByte/100 ) + QString( "GB" ) );
        }
        if( size > MegaByte )
        {
            return QString( QString::number( size / MegaByte, 10 ) + "." + QString::number( size%MegaByte/KiloByte/100 ) + QString( "MB" ) );
        }
        if( size > KiloByte )
        {
            return QString( QString::number( size / KiloByte, 10 ) + QString( "KB" ) );
        }

        return QString( QString::number( size, 10 ) + "B");
    }else
    {
        if( size > GigaByte )
        {
            quint64 gigaBytes = size/GigaByte;
            quint64 remainder = size%GigaByte;
            quint64 remainderAsGB = remainder/MegaByte/100;
            return QString( QString::number( gigaBytes, 10 ) + "." + QString::number( remainderAsGB ) );
            //return QString( QString::number( size / GigaByte, 10 ) + "." + QString::number( size%GigaByte/MegaByte/100 ));
        }
        if( size > MegaByte )
        {
            return QString( QString::number( size / MegaByte, 10 ) + "." + QString::number( size%MegaByte/KiloByte/100 ));
        }
        if( size > KiloByte )
        {
            return QString( QString::number( size / KiloByte, 10 ) );
        }

        return QString( QString::number( size, 10 ) );
    }

    return "0";
}

QString convertFromAmigaTextEncoding( char *text )
{
    const QString utf8_1 = QString::fromLatin1( text );
    return utf8_1;
}

void convertFromUTF8ToAmigaTextEncoding( QString utf8Text, char *encodedText, quint64 encodedTextLength )
{
    QTextCodec *textCodec = QTextCodec::codecForName("ISO 8859-1");
    QByteArray encodedTextArray = textCodec->fromUnicode( utf8Text );
    strncpy( encodedText, encodedTextArray.data(), encodedTextLength );
}
