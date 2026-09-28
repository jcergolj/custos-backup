QT += core testlib
CONFIG += c++17 console testcase
CONFIG -= app_bundle
TEMPLATE = app

SOURCES += \
    backupmanifest_test.cpp \
    ../src/backupmanifest.cpp \
    ../src/backupengine.cpp \
    ../src/localprovider.cpp

HEADERS += \
    ../src/backupmanifest.h \
    ../src/backupengine.h \
    ../src/backupprovider.h
