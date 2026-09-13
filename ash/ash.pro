QT -= gui widgets
QT += core network

CONFIG -= app_bundle
CONFIG += c++17 console
QMAKE_CXXFLAGS_DEBUG += -O0

win32:equals(QMAKE_HOST.os, Windows) {
    WINDEPLOYQT = $$shell_path($$[QT_INSTALL_BINS]/windeployqt.exe)

    CONFIG(debug, debug|release): DEPLOY_EXE = $$shell_path($$OUT_PWD/debug/$${TARGET}.exe)
    CONFIG(release, debug|release): DEPLOY_EXE = $$shell_path($$OUT_PWD/release/$${TARGET}.exe)

    QMAKE_POST_LINK += cmd /c if exist $$quote($$DEPLOY_EXE) $$quote($$WINDEPLOYQT) --compiler-runtime $$quote($$DEPLOY_EXE)
}

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060B01

INCLUDEPATH += ../ ../acp/

SOURCES += \
    main.cpp \
    shellsession.cpp \
    ../acp/AEUtils.cpp \
    ../acp/aeconnection.cpp \
    ../acp/amigahost.cpp \
    ../acp/devicediscovery.cpp \
    ../acp/directorylisting.cpp \
    ../acp/diskvolume.cpp \
    ../acp/hostlister.cpp \
    ../acp/messagepool.cpp \
    ../acp/protocolhandler.cpp

HEADERS += \
    shellsession.h \
    ../protocolTypes.h \
    ../acp/AEUtils.h \
    ../acp/aeconnection.h \
    ../acp/amigahost.h \
    ../acp/devicediscovery.h \
    ../acp/directorylisting.h \
    ../acp/diskvolume.h \
    ../acp/hostlister.h \
    ../acp/messagepool.h \
    ../acp/protocolhandler.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
