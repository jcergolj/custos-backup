QT += core testlib
CONFIG += c++17 console testcase
CONFIG -= app_bundle
TEMPLATE = app

SOURCES += \
    protonprovider_test.cpp \
    ../src/protonprovider.cpp

HEADERS += \
    ../src/backupprovider.h \
    ../src/processrunner.h \
    ../src/protonprovider.h
