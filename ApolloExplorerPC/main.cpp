#include "mainwindow.h"
#include "scanningwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QFontDatabase>
#include "directorylisting.h"
#include "mouseeventfilter.h"

int main(int argc, char *argv[])
{
    //Register the new datatype to QT
    QApplication a(argc, argv);
    qRegisterMetaType<QHostAddress>();
    qRegisterMetaType<QSharedPointer<DirectoryListing>>();

    //Add the mouse event filter
    a.installNativeEventFilter( MouseEventFilterSingleton::getInstance() );

    //Add the fonts
    QFontDatabase::addApplicationFont(":/comfortaa");
    QApplication::setWindowIcon( QIcon( ":/browser/icons/Apollo_Explorer_icon.png" ) );
    a.setWindowIcon( QIcon( ":/browser/icons/Apollo_Explorer_icon.png" ) );


    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "ApolloExplorer_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    //Start the scanning window
    ScanningWindow sw;
    sw.show();
    return a.exec();
}
