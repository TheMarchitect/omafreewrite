QT += core gui network quick testlib
CONFIG += testcase c++17
TEMPLATE = app
TARGET = tst_omafreewrite

INCLUDEPATH += ../src
SOURCES += \
    tst_omafreewrite.cpp \
    ../src/backend.cpp \
    ../src/markdownhighlighter.cpp \
    ../src/polish.cpp
HEADERS += \
    ../src/backend.h \
    ../src/markdownhighlighter.h \
    ../src/polish.h

QT += widgets printsupport quickcontrols2 quickdialogs2 dbus
