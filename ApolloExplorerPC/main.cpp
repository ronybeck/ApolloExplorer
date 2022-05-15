#include "mainwindow.h"
#include "scanningwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include "protocolTypes.h"
#include "directorylisting.h"
#include "mouseeventfilter.h"

int main(int argc, char *argv[])
{
    //Register the new datatype to QT
    //Q_DECLARE_METATYPE( ProtocolMessageType_t );
    //qRegisterMetaType<ProtocolMessageType_t>();
    QApplication a(argc, argv);
    qRegisterMetaType<QHostAddress>();
    qRegisterMetaType<QSharedPointer<DirectoryListing>>();

    //Add the mouse event filter
    a.installNativeEventFilter( MouseEventFilterSingleton::getInstance() );

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "VnetPC_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    //MainWindow w;
    //w.show();

    //Start the scanning window
    ScanningWindow sw;
    sw.show();
    return a.exec();
}
