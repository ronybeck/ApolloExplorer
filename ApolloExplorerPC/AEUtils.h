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

#define SETTINGS_HOSTS "hosts"
#define SETTINGS_BROWSER "Browser"
#define SETTINGS_BROWSER_DOUBLECLICK_ACTION "DoubleClickAction"
#define SETTINGS_SCANNING_WINDOW "ScanningWindow"
#define SETTINGS_WINDOW_WIDTH "WindowWidth"
#define SETTINGS_WINDOW_HEIGHT "WindowHeight"
#define SETTINGS_WINDOW_POSX "WindowX"
#define SETTINGS_WINDOW_POSY "WindowY"
#define SETTINGS_IGNORE "Ignore"
#define SETTINGS_DOWNLOAD "Download"
#define SETTINGS_OPEN "Open"

#endif // AEUTILS_H
