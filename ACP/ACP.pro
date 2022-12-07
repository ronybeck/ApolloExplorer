QT -= gui
QT += network

CONFIG += c++17 console
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
TARGET=acp

SOURCES += \
        ../ApolloExplorerPC/AEUtils.cpp \
        ../ApolloExplorerPC/aeconnection.cpp \
        ../ApolloExplorerPC/amigahost.cpp \
        ../ApolloExplorerPC/devicediscovery.cpp \
        ../ApolloExplorerPC/directorylisting.cpp \
        ../ApolloExplorerPC/downloadthread.cpp \
        ../ApolloExplorerPC/messagepool.cpp \
        ../ApolloExplorerPC/protocolhandler.cpp \
        ../ApolloExplorerPC/uploadthread.cpp \
        main.cpp

TRANSLATIONS += \
    ACP_en_AU.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    ../ApolloExplorerPC/AEUtils.h \
    ../ApolloExplorerPC/aeconnection.h \
    ../ApolloExplorerPC/amigahost.h \
    ../ApolloExplorerPC/devicediscovery.h \
    ../ApolloExplorerPC/directorylisting.h \
    ../ApolloExplorerPC/downloadthread.h \
    ../ApolloExplorerPC/messagepool.h \
    ../ApolloExplorerPC/protocolhandler.h \
    ../ApolloExplorerPC/uploadthread.h \
    ../protocolTypes.h
