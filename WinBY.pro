
QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

VERSION = 2.0.0.1
QMAKE_TARGET_COMPANY     = Azurchik
QMAKE_TARGET_PRODUCT     = WinBY
QMAKE_TARGET_DESCRIPTION = "Archiver WinBY"
QMAKE_TARGET_COPYRIGHT   = "Bazko Yurii"

TARGET = WinBY
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS


CONFIG += c++11

SOURCES += \
        bootwindow.cpp \
        main.cpp \
        mainwindow.cpp \
        winby.cpp

HEADERS += \
        bootwindow.h \
        mainwindow.h \
        winby.h

FORMS += \
        bootwindow.ui \
        mainwindow.ui

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RC_ICONS = icon.ico

RESOURCES += \
    resource.qrc
