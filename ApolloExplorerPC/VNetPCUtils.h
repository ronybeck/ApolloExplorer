#ifndef VNETPCUTILS_H
#define VNETPCUTILS_H

#include <QObject>

QString convertFromAmigaTextEncoding( char *text );
void convertFromUTF8ToAmigaTextEncoding( QString utf8Text, char *encodedText, quint64 encodedTextLength );

#endif // VNETPCUTILS_H
