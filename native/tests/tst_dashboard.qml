import QtQuick
import QtQuick.Controls
import QtTest
import "../qml" as Custos

TestCase {
    id: testCase
    name: "Dashboard"
    when: windowShown
    property var app

    Component { id: windowComponent; Custos.Main {} }

    function init() {
        backupSetController.setNames = ["Documents", "Photos"]
        backupSetController.setIds = ["documents-id", "photos-id"]
        backupSetController.currentIndex = 0
        backupSetController.runningSetIds = []
        backupSetController.removedIndex = -1
        backupSetController.addedCount = 0
        backupSetController.recentBackups = ["Photos\nNo backup run yet", "Documents\nsucceeded"]
        backupSetController.recentBackupSetIds = ["photos-id", "documents-id"]
        backupSetController.recentBackupTimestamps = ["", "2026-10-01 10:00"]
        backupLauncher.launchedId = ""
        restoreController.discoveredRoot = ""
        app = createTemporaryObject(windowComponent, testCase)
        verify(app !== null)
        app.requestActivate()
        waitForRendering(app.contentItem)
    }

    function control(name) {
        let item = null
        tryVerify(function () {
            item = findChild(app, name)
            if (item === null) {
                for (const listName of ["dashboardSetsList", "recentBackupsList"]) {
                    const list = findChild(app, listName)
                    for (let index = 0; list && index < list.count; ++index) {
                        const row = list.itemAtIndex(index)
                        if (row) {
                            item = findChild(row, name)
                            if (item) {
                                return true
                            }
                        }
                    }
                }
            }
            return item !== null
        }, 1000, "Missing UI control: " + name)
        return item
    }

    function test_setsAreLeftOfRecentBackups() {
        compare(control("backupSetsTitle").text, "Backup sets")
        compare(control("recentBackupsTitle").text, "Recent backups")
        compare(control("newBackupSetButton").text, "New backup set")
        const sets = control("dashboardSetsList")
        const recent = control("recentBackupsList")
        const left = sets.mapToItem(app.contentItem, 0, 0)
        const right = recent.mapToItem(app.contentItem, 0, 0)
        verify(left.x + sets.width < right.x, "Backup sets must be in the left column")
        verify(Math.abs(left.y - right.y) < 2, "The two lists must align")
    }

    function openMenu(index) {
        const button = control("setActions-" + index)
        mouseClick(button)
        const menu = control("setMenu-" + index)
        tryCompare(menu, "opened", true)
        return menu
    }

    function test_editLoadsTheMenuTarget() {
        const menu = openMenu(1)
        compare(menu.itemAt(0).text, "Edit")
        mouseClick(menu.itemAt(0))
        tryCompare(app, "showEditor", true)
        compare(backupSetController.currentIndex, 1)
        compare(control("setNameField").text, "Photos")
        compare(backupLauncher.launchedId, "")
    }

    function test_backupLaunchesTheMenuTargetWithoutChangingSelection() {
        const menu = openMenu(1)
        compare(menu.itemAt(1).text, "Back up now")
        mouseClick(menu.itemAt(1))
        compare(backupLauncher.launchedId, "photos-id")
        compare(backupSetController.currentIndex, 0)
        compare(app.showEditor, false)
        compare(control("recentSummary-0").text, "Photos\nNo backup run yet")
        compare(control("restore-0").enabled, false)
    }

    function test_deleteRequiresConfirmationAndCancelDoesNotRemove() {
        const menu = openMenu(1)
        compare(menu.itemAt(3).text, "Delete")
        mouseClick(menu.itemAt(3))
        const dialog = control("removeSetDialog")
        tryCompare(dialog, "opened", true)
        compare(dialog.setName, "Photos")
        compare(backupSetController.removedIndex, -1)
        mouseClick(dialog.standardButton(Dialog.Cancel))
        tryCompare(dialog, "visible", false)
        compare(backupSetController.removedIndex, -1)

        mouseClick(openMenu(1).itemAt(3))
        tryCompare(dialog, "opened", true)
        mouseClick(dialog.standardButton(Dialog.Ok))
        compare(backupSetController.removedIndex, 1)
    }

    function test_runningSetCannotLaunchAgain() {
        backupSetController.runningSetIds = ["photos-id"]
        const indicator = control("setBusy-1")
        tryCompare(indicator, "visible", true)
        compare(indicator.running, true)
        const menu = openMenu(1)
        compare(menu.itemAt(1).enabled, false)
        compare(menu.itemAt(0).enabled, true)
        mouseClick(menu.itemAt(1))
        compare(backupLauncher.launchedId, "")
        menu.close()
        tryCompare(menu, "visible", false)
        compare(openMenu(0).itemAt(1).enabled, true)
    }

    function test_escapeCancelsDeleteConfirmation() {
        mouseClick(openMenu(1).itemAt(3))
        const dialog = control("removeSetDialog")
        tryCompare(dialog, "opened", true)
        keyClick(Qt.Key_Escape)
        tryCompare(dialog, "visible", false)
        compare(backupSetController.removedIndex, -1)
    }

    function test_restoreUsesHistorySetIdAndRequiresActivity() {
        compare(control("restore-0").enabled, false)
        mouseClick(control("restore-0"))
        compare(restoreController.discoveredRoot, "")
        compare(app.showRestore, false)
        backupSetController.currentIndex = 1
        mouseClick(control("restore-1"))
        compare(backupSetController.currentIndex, 0)
        compare(restoreController.discoveredRoot, "/backups/documents-id")
        compare(app.showRestore, true)
        compare(control("recentSummary-1").text, "Documents\nsucceeded")
    }

    function test_unknownHistorySetCannotRestore() {
        backupSetController.recentBackupSetIds = ["photos-id", "removed-id"]
        compare(control("restore-1").enabled, false)
        mouseClick(control("restore-1"))
        compare(restoreController.discoveredRoot, "")
    }

    function test_statusVisibleWithoutRestore_data() {
        return [
            { tag: "set status", source: "set", failure: false },
            { tag: "set failure", source: "set", failure: true },
            { tag: "launcher failure", source: "launcher", failure: true },
            { tag: "restore status", source: "restore", failure: false },
            { tag: "restore failure", source: "restore", failure: true }
        ]
    }

    function test_statusVisibleWithoutRestore(data) {
        const controller = data.source === "set" ? backupSetController
            : data.source === "launcher" ? backupLauncher : restoreController
        const message = data.failure ? "Permission denied" : "Preview ready"
        if (data.failure) {
            controller.failed(message)
        } else {
            controller.statusChanged(message)
        }
        const status = control("dashboardStatusLabel")
        compare(status.text, message)
        verify(status.visible)
        compare(app.showRestore, false)
    }

    function test_launcherStartedStatusVisible() {
        backupLauncher.started()
        compare(control("dashboardStatusLabel").text, "Backup started.")
        verify(control("dashboardStatusLabel").visible)
    }

    function test_newSetStillOpensTheEditor() {
        mouseClick(control("newBackupSetButton"))
        compare(backupSetController.addedCount, 1)
        compare(app.showEditor, true)
        compare(control("setNameField").text, "New set")
    }

    function test_keyboardMenuNavigationAndEscape() {
        const button = control("setActions-1")
        compare(button.Accessible.name, "Actions for Photos")
        button.forceActiveFocus()
        keyClick(Qt.Key_Space)
        const menu = control("setMenu-1")
        tryCompare(menu, "opened", true)
        keyClick(Qt.Key_Escape)
        tryCompare(menu, "visible", false)
        tryCompare(button, "activeFocus", true)
        keyClick(Qt.Key_Space)
        tryCompare(menu, "opened", true)
        keyClick(Qt.Key_Down)
        keyClick(Qt.Key_Return)
        tryCompare(app, "showEditor", true)
        compare(backupSetController.currentIndex, 1)
    }

    function test_outsideClickDismissesMenu() {
        const menu = openMenu(0)
        mouseClick(app.contentItem, app.width - 24, app.height - 24)
        tryCompare(menu, "visible", false)
        compare(backupLauncher.launchedId, "")
        compare(backupSetController.removedIndex, -1)
    }

    function test_emptyDashboardKeepsCreationAvailable() {
        backupSetController.setIds = []
        backupSetController.setNames = []
        backupSetController.recentBackupSetIds = []
        backupSetController.recentBackupTimestamps = []
        backupSetController.recentBackups = []
        tryCompare(control("dashboardSetsList"), "count", 0)
        tryCompare(control("recentBackupsList"), "count", 0)
        verify(control("emptySetsLabel").visible)
        verify(control("emptyRecentLabel").visible)
        verify(control("newBackupSetButton").enabled)
    }

    function test_minimumWindowAndLongHistoryStayWithinColumns() {
        app.width = app.minimumWidth
        app.height = app.minimumHeight
        backupSetController.recentBackups = ["Photos\nfailed | " + "VeryLongUnbrokenFailureMessage".repeat(8), "Documents: succeeded"]
        waitForRendering(app.contentItem)
        const sets = control("dashboardSetsList")
        const recent = control("recentBackupsList")
        verify(sets.width >= 300)
        verify(recent.width >= 300)
        const restore = control("restore-0")
        const position = restore.mapToItem(app.contentItem, 0, 0)
        verify(position.x + restore.width <= app.width - app.contentPadding)
        verify(position.y + restore.height <= app.height)
        const summary = control("recentSummary-0")
        verify(summary.implicitHeight > 40)
        verify(recent.contentHeight >= summary.implicitHeight)
        if (dashboardScreenshotPath.length > 0) {
            grabImage(app.contentItem).save(dashboardScreenshotPath + ".minimum.png")
        }
    }

    function test_manySetsScrollAndLastMenuKeepsItsTarget() {
        app.width = app.minimumWidth
        app.height = app.minimumHeight
        const names = ["Documents", "Photos"]
        const ids = ["documents-id", "photos-id"]
        for (let index = 2; index < 15; ++index) {
            names.push("Backup " + index)
            ids.push("backup-id-" + index)
        }
        backupSetController.setIds = ids
        backupSetController.setNames = names
        const list = control("dashboardSetsList")
        tryCompare(list, "count", 15)
        verify(list.contentHeight > list.height)
        verify(list.height <= 360)
        list.positionViewAtIndex(14, ListView.Contain)
        waitForRendering(app.contentItem)
        const button = control("setActions-14")
        const position = button.mapToItem(list, 0, 0)
        verify(position.y >= 0)
        verify(position.y + button.height <= list.height)
        mouseClick(openMenu(14).itemAt(1))
        compare(backupLauncher.launchedId, "backup-id-14")
    }

    function test_lightPaletteOverridesDarkHost() {
        compare(app.palette.window, "#ffffff")
        compare(app.palette.windowText, "#19232e")
        compare(app.palette.disabled.buttonText, "#586575")
        compare(control("newBackupSetButton").palette.buttonText, "#19232e")
        compare(control("restore-0").palette.disabled.buttonText, "#586575")
        const image = grabImage(app.contentItem)
        compare(image.pixel(app.width - 8, app.height - 8), "#ffffff")
        if (dashboardScreenshotPath.length > 0) {
            image.save(dashboardScreenshotPath)
        }
    }
}
