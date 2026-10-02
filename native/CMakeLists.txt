cmake_minimum_required(VERSION 3.21)

project(omacustos VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 QuickDialogs2 Test Concurrent)

qt_add_executable(omacustos
    src/main.cpp
    src/backupengine.cpp
    src/backupmanifest.cpp
    src/backuprestorecontroller.cpp
    src/backupcatalog.cpp
    src/backupecleanup.cpp
    src/backupsetcontroller.cpp
    src/backupschedule.cpp
    src/backuprunstore.cpp
    src/backupprerequisites.cpp
    src/backupconfig.cpp
    src/systemdlauncher.cpp
    src/localprovider.cpp
    src/qprocessrunner.cpp
    src/protonprovider.cpp
    src/protonfolderbrowser.cpp
    src/recentbackupcopies.cpp
    src/protonauthcontroller.cpp
    src/themecolors.cpp
    src/resourceusage.cpp
    src/backupscheduler.cpp
    src/backupjob.cpp
    src/backupworker.cpp
    src/backuplauncher.cpp
    src/backupengine.h
    src/backupmanifest.h
    src/backuprestorecontroller.h
    src/backupcatalog.h
    src/backupecleanup.h
    src/backupsetcontroller.h
)

qt_add_executable(omacustos-worker
    src/worker_main.cpp
    src/backupconfig.cpp
    src/backupengine.cpp
    src/backupmanifest.cpp
    src/protonprovider.cpp
    src/qprocessrunner.cpp
    src/backupschedule.cpp
    src/backuprunstore.cpp
    src/backupprerequisites.cpp
    src/backupcatalog.cpp
    src/backupecleanup.cpp
)

target_link_libraries(omacustos-worker PRIVATE Qt6::Core)

qt_add_qml_module(omacustos
    URI OmaCustos
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Main.qml
)

target_link_libraries(omacustos PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Qt6::Concurrent
)

enable_testing()

qt_add_executable(qprocessrunner-test
    tests/qprocessrunner_test.cpp
    src/qprocessrunner.cpp
    src/protonprovider.cpp
    src/backupengine.cpp
    src/backupmanifest.cpp
)
target_link_libraries(qprocessrunner-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME qprocessrunner-test COMMAND qprocessrunner-test)
set_tests_properties(qprocessrunner-test PROPERTIES TIMEOUT 30)

qt_add_executable(backupscheduler-test tests/backupscheduler_test.cpp
    src/backupscheduler.cpp src/backupconfig.cpp)
target_link_libraries(backupscheduler-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME backupscheduler-test COMMAND backupscheduler-test)

qt_add_executable(resourceusage-test tests/resourceusage_test.cpp src/resourceusage.cpp)
target_link_libraries(resourceusage-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME resourceusage-test COMMAND resourceusage-test)

qt_add_executable(protonauthcontroller-test
    tests/protonauthcontroller_test.cpp
    src/protonauthcontroller.cpp
)
target_link_libraries(protonauthcontroller-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME protonauthcontroller-test COMMAND protonauthcontroller-test)

qt_add_executable(themecolors-test
    tests/themecolors_test.cpp
    src/themecolors.cpp
)
target_link_libraries(themecolors-test PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME themecolors-test COMMAND themecolors-test)
set_tests_properties(themecolors-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=")

qt_add_executable(backupsetcontroller-test
    tests/backupsetcontroller_test.cpp
    src/backupsetcontroller.cpp
    src/backupengine.cpp
    src/backupconfig.cpp
    src/backupschedule.cpp
    src/backuprunstore.cpp
    src/backupecleanup.cpp
    src/backupcatalog.cpp
    src/backupmanifest.cpp
)

target_link_libraries(backupsetcontroller-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME backupsetcontroller-test COMMAND backupsetcontroller-test)

qt_add_executable(protonfolderbrowser-test
    tests/protonfolderbrowser_test.cpp
    src/protonfolderbrowser.cpp
)
target_link_libraries(protonfolderbrowser-test PRIVATE Qt6::Core Qt6::Test Qt6::Concurrent)
add_test(NAME protonfolderbrowser-test COMMAND protonfolderbrowser-test)

qt_add_executable(recentbackupcopies-test
    tests/recentbackupcopies_test.cpp
    src/recentbackupcopies.cpp
    src/backupconfig.cpp
    src/backupecleanup.cpp
    src/backupschedule.cpp
    src/backuprunstore.cpp
    src/backupmanifest.cpp
    src/backupengine.cpp
    src/localprovider.cpp
)
target_link_libraries(recentbackupcopies-test PRIVATE Qt6::Core Qt6::Test Qt6::Concurrent)
add_test(NAME recentbackupcopies-test COMMAND recentbackupcopies-test)

find_package(Qt6 REQUIRED COMPONENTS QuickTest)
qt_add_executable(dashboard-test tests/dashboard_test.cpp)
target_link_libraries(dashboard-test PRIVATE Qt6::QuickTest Qt6::QuickControls2 Qt6::QuickDialogs2)
target_compile_definitions(dashboard-test PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests")
add_test(NAME dashboard-test COMMAND dashboard-test)
set_tests_properties(dashboard-test PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Fusion;QT_QPA_PLATFORMTHEME=;QT_FORCE_STDERR_LOGGING=1"
)

qt_add_executable(backupengine-test
    tests/backupengine_test.cpp
    src/backupengine.cpp
    src/backupmanifest.cpp
    src/localprovider.cpp
    src/qprocessrunner.cpp
    src/protonprovider.cpp
    src/backupjob.cpp
    src/backupworker.cpp
    src/backupengine.h
    src/backupconfig.h
)

target_link_libraries(backupengine-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME backupengine-test COMMAND backupengine-test)

qt_add_executable(protonprovider-test
    tests/protonprovider_test.cpp
    src/protonprovider.cpp
    src/backupengine.cpp
    src/backupmanifest.cpp
)

target_link_libraries(protonprovider-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME protonprovider-test COMMAND protonprovider-test)

qt_add_executable(protonclifixture-test
    tests/protonclifixture_test.cpp
    tests/protonclifixture.h
    src/protonprovider.cpp
    src/backupengine.cpp
    src/backupmanifest.cpp
)
target_link_libraries(protonclifixture-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME protonclifixture-test COMMAND protonclifixture-test)

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

qt_add_executable(systemdlauncher-test
    tests/systemdlauncher_test.cpp
    src/systemdlauncher.cpp
)

target_link_libraries(systemdlauncher-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME systemdlauncher-test COMMAND systemdlauncher-test)

qt_add_executable(serviceinstaller-test
    tests/serviceinstaller_test.cpp
    src/serviceinstaller.cpp
)

target_link_libraries(serviceinstaller-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME serviceinstaller-test COMMAND serviceinstaller-test)

qt_add_executable(reliability-test
    tests/reliability_test.cpp
    src/backupschedule.cpp
    src/backuprunstore.cpp
    src/backupprerequisites.cpp
    src/backupconfig.cpp
)

target_link_libraries(reliability-test PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(reliability-test PRIVATE OMACUSTOS_WORKER_BINARY="$<TARGET_FILE:omacustos-worker>")
add_dependencies(reliability-test omacustos-worker)
add_test(NAME reliability-test COMMAND reliability-test)

qt_add_executable(workerrecovery-test
    tests/workerrecovery_test.cpp
    src/backuprunstore.cpp
    src/backupconfig.cpp
)
target_link_libraries(workerrecovery-test PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(workerrecovery-test PRIVATE OMACUSTOS_WORKER_BINARY="$<TARGET_FILE:omacustos-worker>")
add_dependencies(workerrecovery-test omacustos-worker)
add_test(NAME workerrecovery-test COMMAND workerrecovery-test)
set_tests_properties(workerrecovery-test PROPERTIES TIMEOUT 60)

qt_add_executable(backupconfig-test
    tests/backupconfig_test.cpp
    src/backupconfig.cpp
)

target_link_libraries(backupconfig-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME backupconfig-test COMMAND backupconfig-test)

qt_add_executable(backupmanifest-test
    tests/backupmanifest_test.cpp
    src/backupmanifest.cpp
    src/backupengine.cpp
    src/localprovider.cpp
)

target_link_libraries(backupmanifest-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME backupmanifest-test COMMAND backupmanifest-test)

qt_add_executable(backuprestorecontroller-test
    tests/backuprestorecontroller_test.cpp
    src/backuprestorecontroller.cpp
    src/backupcatalog.cpp
    src/backupmanifest.cpp
    src/backupengine.cpp
    src/localprovider.cpp
    src/qprocessrunner.cpp
    src/protonprovider.cpp
    src/backupjob.cpp
    src/backupworker.cpp
    src/backuprestorecontroller.h
    src/backupmanifest.h
    src/backupengine.h
)

target_link_libraries(backuprestorecontroller-test PRIVATE Qt6::Core Qt6::Test Qt6::Concurrent Qt6::Qml)
add_test(NAME backuprestorecontroller-test COMMAND backuprestorecontroller-test)

qt_add_executable(retention-discovery-test
    tests/retention_discovery_test.cpp
    src/backupcatalog.cpp
    src/backupecleanup.cpp
    src/backupmanifest.cpp
    src/backupengine.cpp
    src/localprovider.cpp
)

target_link_libraries(retention-discovery-test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME retention-discovery-test COMMAND retention-discovery-test)

qt_add_executable(omacustos-install
    src/install_main.cpp
    src/serviceinstaller.cpp
    src/systemdlauncher.cpp
    src/qprocessrunner.cpp
)

target_link_libraries(omacustos-install PRIVATE Qt6::Core)
