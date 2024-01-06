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
QString prettyFileSize( quint64 size, bool withUnits = true );

#define SETTINGS_HOSTS "hosts"
#define SETTINGS_BROWSER "Browser"
#define SETTINGS_GENERAL "General"
#define SETTINGS_HELLO_TIMEOUT "HelloTimeout"
#define SETTINGS_BROWSER_DOUBLECLICK_ACTION "DoubleClickAction"
#define SETTINGS_BROWSER_DELAY_BETWEEN_DELETES "DelayBetweenDeletes"
#define SETTINGS_DND_SIZE "DNDSize"
#define SETTINGS_DND_OPERATION "DNDOperation"
#define SETTINGS_DND_OPERATION_BLOCK "Block"
#define SETTINGS_DND_OPERATION_DOWNLOAD_DIALOG "Download Dialog"
#define SETTINGS_DND_OPERATION_CONTINUE "Drag and Drop"
#define SETTINGS_SCANNING_WINDOW "ScanningWindow"
#define SETTINGS_SEEN_WHATS_NEW_VERSION "SeenWhatsNewVersion"
#define SETTINGS_WINDOW_WIDTH "WindowWidth"
#define SETTINGS_WINDOW_HEIGHT "WindowHeight"
#define SETTINGS_WINDOW_POSX "WindowX"
#define SETTINGS_WINDOW_POSY "WindowY"
#define SETTINGS_SORTING_DEFAULT "SortingDefault"
#define SETTINGS_SORTING_NAME "Name"
#define SETTINGS_SORTING_TYPE "Type"
#define SETTINGS_SORTING_SIZE "Size"
#define SETTINGS_SORTING_IGNORE_CASE "SortingIgnoreCase"
#define SETTINGS_IGNORE "Ignore"
#define SETTINGS_DOWNLOAD "Download"
#define SETTINGS_OPEN "Open"
#define SETTINGS_ICON_VERTICAL_SIZE "IconVerticalSize"
#define SETTINGS_DOWNLOAD_AMIGA_ICONS "DownloadAmigaIcons"

#endif // AEUTILS_H
