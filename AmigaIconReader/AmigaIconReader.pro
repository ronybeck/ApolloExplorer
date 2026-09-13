QT       += core gui widgets

CONFIG += c++17
QMAKE_CXXFLAGS_DEBUG += -O0

# Copy Qt (and MinGW) DLLs next to the .exe so it runs outside Qt Creator / without PATH.
# https://doc.qt.io/qt-6/windows-deployment.html
# Note: Do not pass --debug to windeployqt for MinGW kits - prebuilt Qt ships release
# Qt DLLs/plugins only; --debug expects qwindowsd.dll etc. and fails.
win32:equals(QMAKE_HOST.os, Windows) {
    WINDEPLOYQT = $$shell_path($$[QT_INSTALL_BINS]/windeployqt.exe)

    CONFIG(debug, debug|release): DEPLOY_EXE = $$shell_path($$OUT_PWD/debug/$${TARGET}.exe)
    CONFIG(release, debug|release): DEPLOY_EXE = $$shell_path($$OUT_PWD/release/$${TARGET}.exe)

    QMAKE_POST_LINK += cmd /c if exist $$quote($$DEPLOY_EXE) $$quote($$WINDEPLOYQT) --compiler-runtime $$quote($$DEPLOY_EXE)
}

# Fail the build on APIs Qt marked deprecated before this version (6.11.1 => 0x060B01). Adjust when upgrading Qt.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060B01

SOURCES += \
    AmigaInfoFile.cpp \
    bitgetter.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    AmigaInfoFile.h \
    bitgetter.h \
    mainwindow.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
