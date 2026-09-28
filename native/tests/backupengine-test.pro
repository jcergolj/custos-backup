QT += core testlib
CONFIG += c++17 console testcase
CONFIG -= app_bundle
TEMPLATE = app

SOURCES += \
    backupengine_test.cpp \
    ../src/backupengine.cpp

HEADERS += \
    ../src/backupengine.h
