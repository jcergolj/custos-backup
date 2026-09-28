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
    src/backupjob.cpp
    src/backupworker.cpp

HEADERS += \
    src/backupengine.h \
    src/backupprovider.h \
    src/localprovider.h \
    src/processrunner.h \
    src/qprocessrunner.h \
    src/protonprovider.h
    src/backupjob.h
    src/backupworker.h

RESOURCES += qml.qrc

TARGET = praefectus-native

worker {
    TARGET = praefectus-native-worker
    SOURCES = \
        src/worker_main.cpp \
        src/backupengine.cpp \
        src/protonprovider.cpp \
        src/qprocessrunner.cpp
    HEADERS = \
        src/backupengine.h \
        src/backupprovider.h \
        src/processrunner.h \
        src/protonprovider.h \
        src/qprocessrunner.h
}
