######################################################################
# Automatically generated by qmake (2.01a) Fri Nov 30 09:59:50 2007
######################################################################

TEMPLATE = lib
CONFIG += plugin release threads
TARGET = leechcraft_updater
DESTDIR = ../bin
DEPENDPATH += .
INCLUDEPATH += ../../
INCLUDEPATH += .
QT += xml
TRANSLATIONS += leechcraft_updater_ru.ts
HEADERS += updaterplugin.h globals.h settingsmanager.h core.h
SOURCES += updaterplugin.cpp settingsmanager.cpp core.cpp
RESOURCES += resources.qrc
win32:LIBS += -L../.. -lplugininterface -lexceptions -lsettingsdialog
