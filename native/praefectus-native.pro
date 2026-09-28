QT += core gui qml quick quickcontrols2
CONFIG += c++17 console
CONFIG -= app_bundle
TEMPLATE = app

SOURCES += \
    src/main.cpp \
    src/backupengine.cpp \
    src/localprovider.cpp \
    src/qprocessrunner.cpp \
    src/protonprovider.cpp

HEADERS += \
    src/backupengine.h \
    src/backupprovider.h \
    src/localprovider.h \
    src/processrunner.h \
    src/qprocessrunner.h \
    src/protonprovider.h

RESOURCES += qml.qrc
