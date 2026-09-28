QT += core testlib
CONFIG += c++17 console testcase
CONFIG -= app_bundle
TEMPLATE = app

SOURCES += \
    serviceinstaller_test.cpp \
    ../src/serviceinstaller.cpp

HEADERS += \
    ../src/serviceinstaller.h
