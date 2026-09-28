QT += core testlib
CONFIG += c++17 console testcase
CONFIG -= app_bundle
TEMPLATE = app

SOURCES += \
    backupconfig_test.cpp \
    ../src/backupconfig.cpp

HEADERS += \
    ../src/backupconfig.h
