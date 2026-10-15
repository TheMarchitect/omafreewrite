QT += core gui network widgets printsupport qml quick quickcontrols2 quickdialogs2 dbus

CONFIG += c++17 release
TARGET = omafreewrite
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/markdownhighlighter.h \
    src/polish.h \
    src/systemtheme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/markdownhighlighter.cpp \
    src/polish.cpp \
    src/systemtheme.cpp

RESOURCES += src/resources.qrc
