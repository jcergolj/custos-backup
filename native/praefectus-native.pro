QT += core gui qml quick quickcontrols2
CONFIG += c++17 console
CONFIG -= app_bundle
TEMPLATE = app

SOURCES += \
    src/main.cpp \
    src/backupengine.cpp

HEADERS += \
    src/backupengine.h

RESOURCES += qml.qrc
