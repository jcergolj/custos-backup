cmake_minimum_required(VERSION 3.21)

project(praefectus_native VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Test)

qt_add_executable(praefectus-native
    src/main.cpp
    src/backupengine.cpp
    src/localprovider.cpp
    src/qprocessrunner.cpp
    src/protonprovider.cpp
    src/backupjob.cpp
    src/backupworker.cpp
    src/backupengine.h
)

qt_add_qml_module(praefectus-native
    URI Praefectus
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_link_libraries(praefectus-native PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
)

enable_testing()

qt_add_executable(backupengine-test
    tests/backupengine_test.cpp
    src/backupengine.cpp
    src/localprovider.cpp
    src/qprocessrunner.cpp
    src/protonprovider.cpp
    src/backupjob.cpp
    src/backupworker.cpp
    src/backupengine.h
)

target_link_libraries(backupengine-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME backupengine-test COMMAND backupengine-test)

qt_add_executable(protonprovider-test
    tests/protonprovider_test.cpp
    src/protonprovider.cpp
)

target_link_libraries(protonprovider-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME protonprovider-test COMMAND protonprovider-test)

qt_add_executable(backupjob-test
    tests/backupjob_test.cpp
    src/backupjob.cpp
    src/backupengine.cpp
    src/localprovider.cpp
)

target_link_libraries(backupjob-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME backupjob-test COMMAND backupjob-test)

qt_add_executable(backupworker-test
    tests/backupworker_test.cpp
    src/backupworker.cpp
    src/backupjob.cpp
    src/backupengine.cpp
    src/localprovider.cpp
)

target_link_libraries(backupworker-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME backupworker-test COMMAND backupworker-test)
