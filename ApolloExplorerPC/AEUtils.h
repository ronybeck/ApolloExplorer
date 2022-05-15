#ifndef AEUTILS_H
#define AEUTILS_H

#include <QObject>
#include <QDebug>
#include <QNoDebug>

#if DEBUG > 0
#define DBGLOG  qDebug().noquote() << __PRETTY_FUNCTION__
#else
#define DBGLOG QNoDebug()
#endif
#define WARNLOG qWarning().noquote() << __PRETTY_FUNCTION__

QString convertFromAmigaTextEncoding( char *text );
void convertFromUTF8ToAmigaTextEncoding( QString utf8Text, char *encodedText, quint64 encodedTextLength );

#endif // AEUTILS_H
