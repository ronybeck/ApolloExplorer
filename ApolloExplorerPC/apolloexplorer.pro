QT       += core gui network
TARGET=ApolloExplorer

linux: QT += x11extras
linux: LIBS += -lX11
macos: LIBS += -framework AppKit

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
QMAKE_CXXFLAGS += -O0 -g3

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += ../

SOURCES += \
    AEUtils.cpp \
    aeconnection.cpp \
    amigahost.cpp \
    devicediscovery.cpp \
    dialogconsole.cpp \
    dialogdelete.cpp \
    dialogdownloadfile.cpp \
    dialogpreferences.cpp \
    dialoguploadfile.cpp \
    directorylisting.cpp \
    downloadthread.cpp \
    main.cpp \
    mainwindow.cpp \
    messagepool.cpp \
    protocolhandler.cpp \
    qdragremote.cpp \
    remotefilemimedata.cpp \
    remotefiletablemodel.cpp \
    remotefiletableview.cpp \
    scanningwindow.cpp \
    uploadthread.cpp

linux:    SOURCES += mouseeventfilter.cpp
win:    SOURCES += mouseeventfilter.cpp
macos:  SOURCES += macos_mouseeventfilter.mm

HEADERS += \
    ../protocolTypes.h \
    AEUtils.h \
    aeconnection.h \
    amigahost.h \
    devicediscovery.h \
    dialogconsole.h \
    dialogdelete.h \
    dialogdownloadfile.h \
    dialogpreferences.h \
    dialoguploadfile.h \
    directorylisting.h \
    downloadthread.h \
    mainwindow.h \
    messagepool.h \
    protocolhandler.h \
    qdragremote.h \
    remotefilemimedata.h \
    remotefiletablemodel.h \
    remotefiletableview.h \
    scanningwindow.h \
    uploadthread.h \
    mouseeventfilter.h



FORMS += \
    dialogconsole.ui \
    dialogdelete.ui \
    dialogdownloadfile.ui \
    dialogpreferences.ui \
    dialoguploadfile.ui \
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
    icons/Computer_Amiga.png \
    icons/Computer_Generic.png \
    icons/Computer_Linux.png \
    icons/Computer_MacOS.png \
    icons/directory.png \
    icons/file.png \
    icons/go-up.png

RESOURCES += \
    Fonts.qrc \
    icons.qrc
