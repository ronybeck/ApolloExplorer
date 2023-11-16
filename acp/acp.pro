QT -= gui
QT += core network widgets
CONFIG += c++11 console static
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        AEUtils.cpp \
        aeconnection.cpp \
        amigahost.cpp \
        deletionthread.cpp \
        devicediscovery.cpp \
        directorylister.cpp \
        directorylisting.cpp \
        diskvolume.cpp \
        downloadthread.cpp \
        fileuploader.cpp \
        hostlister.cpp \
        main.cpp \
        messagepool.cpp \
        protocolhandler.cpp \
        uploadthread.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

INCLUDEPATH += ../

HEADERS += \
    ../protocolTypes.h \
    AEUtils.h \
    aeconnection.h \
    amigahost.h \
    deletionthread.h \
    devicediscovery.h \
    directorylister.h \
    directorylisting.h \
    diskvolume.h \
    downloadthread.h \
    fileuploader.h \
    hostlister.h \
    messagepool.h \
    protocolhandler.h \
    uploadthread.h
