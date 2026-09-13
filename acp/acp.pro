QT -= gui widgets
QT += core network

CONFIG -= app_bundle
CONFIG += c++17 console
QMAKE_CXXFLAGS_DEBUG += -O0

# Copy Qt (and MinGW) DLLs next to the .exe so it runs outside Qt Creator / without PATH.
# https://doc.qt.io/qt-6/windows-deployment.html
# Note: Do not pass --debug to windeployqt for MinGW kits - prebuilt Qt ships release
# Qt DLLs/plugins only; --debug expects qwindowsd.dll etc. and fails.
#
# Run windeployqt for both debug\ and release\ outputs when each .exe exists. With the
# Debug kit, qmake only embeds the debug deploy if we gate on CONFIG; then Release
# never gets DLLs until a Release link happens. Deploying whichever exe exists fixes that.
win32:equals(QMAKE_HOST.os, Windows) {
    WINDEPLOYQT = $$shell_path($$[QT_INSTALL_BINS]/windeployqt.exe)

    CONFIG(debug, debug|release): DEPLOY_EXE = $$shell_path($$OUT_PWD/debug/$${TARGET}.exe)
    CONFIG(release, debug|release): DEPLOY_EXE = $$shell_path($$OUT_PWD/release/$${TARGET}.exe)

    QMAKE_POST_LINK += cmd /c if exist $$quote($$DEPLOY_EXE) $$quote($$WINDEPLOYQT) --compiler-runtime $$quote($$DEPLOY_EXE)
}

# Fail the build on APIs Qt marked deprecated before this version (6.11.1 => 0x060B01). Adjust when upgrading Qt.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060B01

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
        filedownloader.cpp \
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
    filedownloader.h \
    fileuploader.h \
    hostlister.h \
    messagepool.h \
    protocolhandler.h \
    uploadthread.h
