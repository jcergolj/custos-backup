QT += core testlib
CONFIG += c++17 console testcase
CONFIG -= app_bundle
TEMPLATE = app

SOURCES += \
    systemdlauncher_test.cpp \
    ../src/systemdlauncher.cpp

HEADERS += \
    ../src/processrunner.h \
    ../src/systemdlauncher.h
