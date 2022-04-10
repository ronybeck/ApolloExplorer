QT       += core gui network
TARGET=ApolloExplorer

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
QMAKE_CXXFLAGS += -O0 -g3

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += ../

SOURCES += \
    VNetPCUtils.cpp \
    devicediscovery.cpp \
    dialogconsole.cpp \
    dialogdelete.cpp \
    dialogdownloadfile.cpp \
    dialoguploadfile.cpp \
    directorylisting.cpp \
    downloadthread.cpp \
    main.cpp \
    mainwindow.cpp \
    messagepool.cpp \
    protocolhandler.cpp \
    scanningwindow.cpp \
    uploadthread.cpp \
    vnetconnection.cpp \
    vnethost.cpp

HEADERS += \
    ../protocolTypes.h \
    VNetPCUtils.h \
    devicediscovery.h \
    dialogconsole.h \
    dialogdelete.h \
    dialogdownloadfile.h \
    dialoguploadfile.h \
    directorylisting.h \
    downloadthread.h \
    mainwindow.h \
    messagepool.h \
    protocolhandler.h \
    scanningwindow.h \
    uploadthread.h \
    vnetconnection.h \
    vnethost.h

FORMS += \
    dialogconsole.ui \
    dialogdelete.ui \
    dialogdownloadfile.ui \
    dialoguploadfile.ui \
    mainwindow.ui \
    scanningwindow.ui

TRANSLATIONS += \
    VnetPC_en_AU.ts
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
    icons/file.png

RESOURCES += \
    icons.qrc
