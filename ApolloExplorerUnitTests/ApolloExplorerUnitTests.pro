QT += testlib

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle

TEMPLATE = app

INCLUDEPATH +=  ../ApolloExplorerPC/ \
                ../

SOURCES +=  tst_directorylisting_setpath.cpp \
    ../ApolloExplorerPC/AEUtils.cpp \
    ../ApolloExplorerPC/directorylisting.cpp

HEADERS += \
    ../ApolloExplorerPC/AEUtils.h \
    ../ApolloExplorerPC/directorylisting.h \
    ../protocolTypes.h
