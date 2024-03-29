QT       += core gui network
TARGET=ApolloExplorer

linux: QT += x11extras
linux: LIBS += -lX11

win32:RC_ICONS += icons/FirebirdHW.ico

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
QMAKE_CXXFLAGS += -O0 -g

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += ../

SOURCES += \
    ../AmigaIconReader/AmigaInfoFile.cpp \
    ../AmigaIconReader/bitgetter.cpp \
    AEUtils.cpp \
    aboutdialog.cpp \
    aeconnection.cpp \
    amigahost.cpp \
    deletionthread.cpp \
    devicediscovery.cpp \
    dialogconsole.cpp \
    dialogdelete.cpp \
    dialogdownloadfile.cpp \
    dialogfileinfo.cpp \
    dialogpreferences.cpp \
    dialoguploadfile.cpp \
    dialogwhatsnew.cpp \
    directorylisting.cpp \
    diskvolume.cpp \
    downloadthread.cpp \
    iconcache.cpp \
    iconthread.cpp \
    main.cpp \
    mainwindow.cpp \
    messagepool.cpp \
    mouseeventfilter.cpp \
    protocolhandler.cpp \
    qdragremote.cpp \
    remotefilelistview.cpp \
    remotefilemimedata.cpp \
    remotefiletablemodel.cpp \
    remotefiletableview.cpp \
    scanningwindow.cpp \
    uploadthread.cpp

HEADERS += \
    ../AmigaIconReader/AmigaInfoFile.h \
    ../AmigaIconReader/bitgetter.h \
    ../protocolTypes.h \
    AEUtils.h \
    aboutdialog.h \
    aeconnection.h \
    amigahost.h \
    deletionthread.h \
    devicediscovery.h \
    dialogconsole.h \
    dialogdelete.h \
    dialogdownloadfile.h \
    dialogfileinfo.h \
    dialogpreferences.h \
    dialoguploadfile.h \
    dialogwhatsnew.h \
    directorylisting.h \
    diskvolume.h \
    downloadthread.h \
    iconcache.h \
    iconthread.h \
    mainwindow.h \
    messagepool.h \
    mouseeventfilter.h \
    protocolhandler.h \
    qdragremote.h \
    remotefilelistview.h \
    remotefilemimedata.h \
    remotefiletablemodel.h \
    remotefiletableview.h \
    scanningwindow.h \
    uploadthread.h

FORMS += \
    aboutdialog.ui \
    dialogconsole.ui \
    dialogdelete.ui \
    dialogdownloadfile.ui \
    dialogfileinfo.ui \
    dialogpreferences.ui \
    dialoguploadfile.ui \
    dialogwhatsnew.ui \
    mainwindow.ui \
    scanningwindow.ui

TRANSLATIONS += \
    ApolloExplorer_en_AU.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    ../LogoBig.png \
    icons/Computer_Amiga.png \
    icons/Computer_Generic.png \
    icons/Computer_Linux.png \
    icons/Computer_MacOS.png \
    icons/directory.png \
    icons/file.png \
    icons/go-up.png \
    icons/refresh.png \
    icons/VampireHW.png \
    icons/IcedrakeHW.png \
    icons/FirebirdHW.png \
    icons/Apollo_Explorer_icon.png \
    icons/Apollo_Explorer_24x24.png

RESOURCES += \
    Fonts.qrc \
    Images.qrc \
    icons.qrc
