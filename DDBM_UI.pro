#-------------------------------------------------
#
# Project created by QtCreator 2011-09-10T12:08:21
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = DDBM_UI
TEMPLATE = app

INCLUDEPATH += $$PWD/ddbm_lib/h/
INCLUDEPATH += $$PWD/ddbm_lib/src/

SOURCES += main.cpp\
    devinfodialog.cpp \
        mainwindow.cpp \
    ddbm_lib/src/DiskIO.cpp \
    ddbm_lib/src/Ddbm_MemBlk.cpp \
    ddbm_lib/src/Ddbm_Corelib.cpp \
    ddbm_lib/src/Ddbm_Bbm.cpp \
    renderarea.cpp

HEADERS  += mainwindow.h \
    ddbm_lib/h/DiskIO.h \
    ddbm_lib/h/Ddbm_Partition.h \
    ddbm_lib/h/Ddbm_MemBlk.h \
    ddbm_lib/h/Ddbm_Corelib.h \
    ddbm_lib/h/Ddbm_Common.h \
    ddbm_lib/h/Ddbm_Bbm.h \
    devinfodialog.h \
    renderarea.h

FORMS    += mainwindow.ui \
    devinfodialog.ui
CONFIG   += console
