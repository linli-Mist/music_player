QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = music_player
TEMPLATE = app

CONFIG += c++11

SOURCES += \
        main.cpp \
        mainwindow.cpp \
        customkeyboard.cpp

HEADERS += \
        mainwindow.h \
        customkeyboard.h

FORMS += \
        mainwindow.ui

unix {
    TARGET = music_player
}
