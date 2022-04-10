#include "VNetPCUtils.h"

#include <QTextEncoder>
#include <QDebug>

QString convertFromAmigaTextEncoding( char *text )
{
    const QString utf8_1 = QString::fromLatin1( text );
    return utf8_1;
    /*
    QTextCodec *tc = QTextCodec::codecForName("CP1252");
    const QByteArray ba = tc->fromUnicode(utf8_1);
    const QString utf8_2 = QString::fromUtf8(ba);
    return utf8_2;
    */
}

void convertFromUTF8ToAmigaTextEncoding( QString utf8Text, char *encodedText, quint64 encodedTextLength )
{
    QTextCodec *textCodec = QTextCodec::codecForName("ISO 8859-1");
    QByteArray encodedTextArray = textCodec->fromUnicode( utf8Text );
    strncpy( encodedText, encodedTextArray.data(), encodedTextLength );
}
