QT += core testlib
CONFIG += c++17 console testcase
CONFIG -= app_bundle
TEMPLATE = app

SOURCES += \
    protonprovider_test.cpp \
    ../src/protonprovider.cpp \
    ../src/backupengine.cpp \
    ../src/backupmanifest.cpp

HEADERS += \
    protonclifixture.h \
    ../src/backupprovider.h \
    ../src/processrunner.h \
    ../src/protonprovider.h \
    ../src/backupengine.h \
    ../src/backupmanifest.h
