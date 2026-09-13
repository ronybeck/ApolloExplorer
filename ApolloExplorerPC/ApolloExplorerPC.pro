QT       += core gui network widgets
TARGET=ApolloExplorer

#linux: QT += x11extras
#linux: LIBS += -lX11

win32:RC_ICONS += icons/FirebirdHW.ico

CONFIG += c++17
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
    icons/IceDrakeHW.png \
    icons/FireBirdHW.png \
    icons/MantiCoreHW.png \
    icons/UniCornHW.png \
    icons/Apollo_Explorer_icon.png \
    icons/Apollo_Explorer_24x24.png

RESOURCES += \
    Fonts.qrc \
    Images.qrc \
    icons.qrc
