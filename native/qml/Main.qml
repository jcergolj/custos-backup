import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "LocalPaths.js" as LocalPaths

ApplicationWindow {
    id: root
    visible: true
    width: 960
    height: 640
    minimumWidth: 760
    minimumHeight: 480
    title: qsTr("OmaCustos")
    property bool showEditor: false
    property bool showRestore: false
    property string notificationMessage: ""

    // Keep the window's dashboard/test API while state lives with its presentation.
    property alias selectedRestoreIndexes: restorePanel.selectedIndexes
    property alias selectedRestorePaths: restorePanel.selectedPaths
    property alias selectedRestoreCopyIndex: restorePanel.selectedCopyIndex
    property alias syncingCurrentSet: backupEditor.syncingCurrentSet
    property alias showAdvanced: backupEditor.showAdvanced
    readonly property bool backupRunning: backupEditor.backupRunning
    property alias systemFontFamily: uiStyle.systemFontFamily
    property alias displayFontFamily: uiStyle.displayFontFamily
    property alias bodyFontFamily: uiStyle.bodyFontFamily
    property alias displayTypeSize: uiStyle.displayTypeSize
    property alias pageTitleSize: uiStyle.pageTitleSize
    property alias sectionTitleSize: uiStyle.sectionTitleSize
    property alias bodyTypeSize: uiStyle.bodyTypeSize
    property alias metadataTypeSize: uiStyle.metadataTypeSize
    property alias bodyLeading: uiStyle.bodyLeading
    property alias readableMeasure: uiStyle.readableMeasure
    property alias contentPadding: uiStyle.contentPadding
    property alias cardPadding: uiStyle.cardPadding
    readonly property color backgroundColor: uiStyle.backgroundColor
    readonly property color inkColor: uiStyle.inkColor
    readonly property color mutedColor: uiStyle.mutedColor
    readonly property color lineColor: uiStyle.lineColor
    readonly property color softColor: uiStyle.softColor
    readonly property color accentColor: uiStyle.accentColor

    UiStyle {
        id: uiStyle
        colors: themeColors.colors
    }

    font.family: bodyFontFamily
    font.pixelSize: bodyTypeSize
    color: backgroundColor
    palette {
        window: root.backgroundColor
        windowText: root.inkColor
        base: root.backgroundColor
        alternateBase: root.softColor
        text: root.inkColor
        button: root.backgroundColor
        buttonText: root.inkColor
        brightText: themeColors.colors.brightText
        highlight: themeColors.colors.highlight
        highlightedText: themeColors.colors.highlightedText
        placeholderText: root.mutedColor
        light: root.softColor
        midlight: root.softColor
        mid: root.lineColor
        dark: root.lineColor
        shadow: root.lineColor
        toolTipBase: root.softColor
        toolTipText: root.inkColor
        accent: root.accentColor
        link: root.accentColor
        linkVisited: root.accentColor
    }

    function loadCurrentSet() {
        backupEditor.loadCurrentSet()
    }

    function localPath(url) {
        return LocalPaths.localPath(url)
    }

    function createNewSet() {
        backupSetController.addSet()
        loadCurrentSet()
        showAdvanced = false
        showEditor = true
    }

    function editSet(index) {
        backupSetController.currentIndex = index
        loadCurrentSet()
        showAdvanced = false
        showEditor = true
        Qt.callLater(function () { backupEditor.focusName() })
    }

    function restoreRecentBackup(index) {
        const setId = backupSetController.recentBackupSetIds[index]
        const setIndex = backupSetController.setIds.indexOf(setId)
        if (setIndex < 0) {
            return
        }

        backupSetController.currentIndex = setIndex
        restorePanel.reset()
        showRestore = true
        restoreController.discover(backupSetController.recentBackupFolderPath(setId), setId)
        Qt.callLater(function () {
            restorePanel.focusSearch()
            dashboardScrollView.contentItem.contentY = Math.max(0,
                restorePanel.mapToItem(dashboardScrollView.contentItem, 0, 0).y)
        })
    }

    function openRecentBackupFolder(index) {
        const setId = backupSetController.recentBackupSetIds[index]
        recentBackupCopies.openCopy(setId)
    }

    function requestRemoveSet(index) {
        removeSetDialog.setIndex = index
        removeSetDialog.setId = backupSetController.setIds[index]
        removeSetDialog.setName = backupSetController.setNames[index]
        removeSetDialog.open()
    }

    function setStatus(message) {
        notificationMessage = message
        if (message.length === 0) {
            notificationTimer.stop()
            notificationToast.close()
        } else {
            notificationToast.open()
            notificationTimer.restart()
        }
    }

    Timer {
        id: notificationTimer
        interval: 5000
        onTriggered: notificationToast.close()
    }

    FileDialog {
        id: importSetsDialog
        objectName: "importSetsDialog"
        title: qsTr("Import backup sets (replace current list)")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("OmaCustos backup sets (*.json)")]
        onAccepted: {
            if (backupSetController.importSets(root.localPath(selectedFile))) {
                root.showEditor = false
                root.loadCurrentSet()
            }
        }
    }

    FileDialog {
        id: exportSetsDialog
        objectName: "exportSetsDialog"
        title: qsTr("Export backup sets")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("OmaCustos backup sets (*.json)")]
        onAccepted: backupSetController.exportSets(root.localPath(selectedFile))
    }

    Popup {
        id: notificationToast
        objectName: "notificationToast"
        parent: Overlay.overlay
        x: root.width - width - root.contentPadding
        y: root.contentPadding
        width: Math.min(360, root.width - 2 * root.contentPadding)
        padding: 16
        closePolicy: Popup.NoAutoClose
        modal: false
        focus: false
        background: Rectangle {
            color: root.softColor
            radius: 8
            border.color: root.lineColor
        }
        contentItem: Label {
            objectName: "notificationMessageLabel"
            text: root.notificationMessage
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }
    }

    function updateDisabledPalette() {
        // Shared palette roles update every color group; apply disabled roles last.
        palette.disabled.text = mutedColor
        palette.disabled.windowText = mutedColor
        palette.disabled.buttonText = mutedColor
        palette.disabled.button = softColor
    }

    Component.onCompleted: {
        updateDisabledPalette()
        showEditor = false
    }

    Dialog {
        id: removeSetDialog
        objectName: "removeSetDialog"
        anchors.centerIn: parent
        property int setIndex: -1
        property string setId: ""
        property string setName: ""
        title: qsTr("Delete backup set")
        width: Math.min(root.width - 2 * root.contentPadding, 420)
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: Label {
            text: qsTr("Delete the \"%1\" backup set from OmaCustos? Its configuration and schedule will be removed. Copies already stored in Proton Drive will remain.").arg(removeSetDialog.setName)
            font.pixelSize: root.bodyTypeSize
            lineHeight: root.bodyLeading
            lineHeightMode: Text.ProportionalHeight
            wrapMode: Text.WordWrap
            padding: 16
        }

        onAccepted: {
            const index = backupSetController.setIds.indexOf(setId)
            if (index >= 0) {
                backupSetController.removeSet(index)
            }
            setIndex = -1
            setId = ""
            setName = ""
        }

        onRejected: {
            setIndex = -1
            setId = ""
            setName = ""
        }
        onOpened: standardButton(Dialog.Ok).text = qsTr("Delete")
    }

    Dialog {
        id: deleteCopyDialog
        objectName: "deleteCopyDialog"
        anchors.centerIn: parent
        property string backupName: ""
        property string copyPath: ""
        title: qsTr("Delete backup copy")
        width: Math.min(root.width - 2 * root.contentPadding, 480)
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label {
            text: qsTr("Delete this copy of \"%1\" from Proton Drive? The copy and all its files will be moved to Proton Drive Trash. Your backup set and other copies will remain.\n\n%2")
                .arg(deleteCopyDialog.backupName).arg(deleteCopyDialog.copyPath)
            padding: 16
            wrapMode: Text.Wrap
        }
        onOpened: standardButton(Dialog.Ok).text = qsTr("Delete copy")
        onAccepted: recentBackupCopies.confirmDelete()
        onRejected: recentBackupCopies.cancelDelete()
    }

    Dialog {
        id: backupDetailsDialog
        objectName: "backupDetailsDialog"
        anchors.centerIn: parent
        property string setId: ""
        readonly property var details: backupSetController.runDetails[setId] || ({})
        title: qsTr("Backup details")
        width: Math.min(root.width - 2 * root.contentPadding, 640)
        height: Math.min(root.height - 2 * root.contentPadding, 500)
        modal: true
        standardButtons: Dialog.Close

        contentItem: ScrollView {
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: 12
                Label {
                    objectName: "backupDetailsStatus"
                    text: backupDetailsDialog.details.status || ""
                    font.weight: Font.DemiBold
                    textFormat: Text.PlainText
                }
                Label {
                    objectName: "backupDetailsSummary"
                    text: backupDetailsDialog.details.summary || ""
                    visible: text.length > 0
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Label {
                    objectName: "backupDetailsError"
                    text: backupDetailsDialog.details.error || ""
                    visible: text.length > 0
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                }
                Label {
                    text: qsTr("Next retry: %1").arg(backupDetailsDialog.details.nextAttempt || "")
                    visible: (backupDetailsDialog.details.nextAttempt || "").length > 0
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Label {
                    text: backupDetailsDialog.details.copyPath || ""
                    visible: text.length > 0
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                    color: root.mutedColor
                    Layout.fillWidth: true
                }
                Repeater {
                    objectName: "backupIssues"
                    model: backupDetailsDialog.details.issues || []
                    delegate: ColumnLayout {
                        id: issueRow
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        spacing: 4
                        Label {
                            objectName: "backupIssuePath-" + issueRow.index
                            text: issueRow.modelData.path
                            textFormat: Text.PlainText
                            font.weight: Font.DemiBold
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }
                        Label {
                            objectName: "backupIssueReason-" + issueRow.index
                            text: qsTr("%1: %2").arg(issueRow.modelData.phase).arg(issueRow.modelData.reason)
                            textFormat: Text.PlainText
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: root.contentPadding
            Layout.rightMargin: root.contentPadding
            Layout.topMargin: 24
            Layout.bottomMargin: 16
            spacing: 12

            Label {
                text: qsTr("OmaCustos")
                font.family: root.displayFontFamily
                font.pixelSize: root.displayTypeSize
                font.weight: Font.Bold
                font.letterSpacing: 0.4
                color: root.inkColor
                Layout.fillWidth: true
            }

            ActionButton {
                style: uiStyle
                objectName: "importSetsButton"
                text: qsTr("Import")
                Layout.preferredHeight: 36
                enabled: !recentBackupCopies.busy
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Load backup sets from a JSON export")
                onClicked: importSetsDialog.open()
            }

            ActionButton {
                style: uiStyle
                objectName: "exportSetsButton"
                text: qsTr("Export")
                Layout.preferredHeight: 36
                enabled: backupSetController.setNames.length > 0
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Export saved backup sets to a JSON file")
                onClicked: exportSetsDialog.open()
            }
        }

        RowLayout {
            objectName: "protonErrorRow"
            visible: protonAuth.checked && !protonAuth.authenticated && protonAuth.error.length > 0
            Layout.fillWidth: true
            Layout.leftMargin: root.contentPadding
            Layout.rightMargin: root.contentPadding
            Layout.bottomMargin: 12
            spacing: 12

            Label {
                objectName: "protonErrorLabel"
                text: protonAuth.error
                color: root.inkColor
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }

            ActionButton {
                style: uiStyle
                objectName: "protonSignInButton"
                text: qsTr("Sign in to Proton")
                visible: protonAuth.cliAvailable
                enabled: !protonAuth.checking
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Complete sign-in in your browser. Keep the terminal open until it finishes.")
                onClicked: protonAuth.signIn()
            }

            ActionButton {
                style: uiStyle
                objectName: "protonAuthRetryButton"
                text: qsTr("Retry")
                enabled: !protonAuth.checking
                onClicked: protonAuth.refresh()
            }
        }

        RowLayout {
            objectName: "schedulingErrorRow"
            visible: backupScheduler.error.length > 0
            Layout.fillWidth: true
            Layout.leftMargin: root.contentPadding
            Layout.rightMargin: root.contentPadding
            Layout.bottomMargin: 12
            spacing: 12

            Label {
                objectName: "schedulingErrorLabel"
                text: backupScheduler.error
                color: root.inkColor
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }

            ActionButton {
                style: uiStyle
                objectName: "enableSchedulingButton"
                text: qsTr("Enable scheduling")
                visible: backupScheduler.hasSchedules && !backupScheduler.ready
                enabled: !backupScheduler.busy && !resourceUsage.busy
                onClicked: backupScheduler.enable()
            }
        }

        StackLayout {
            currentIndex: root.showEditor ? 1 : 0
            Layout.fillWidth: true
            Layout.fillHeight: true

            ScrollView {
                id: dashboardScrollView
                objectName: "dashboardScrollView"
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: root.contentPadding
                    anchors.rightMargin: root.contentPadding
                    spacing: 20

                    Dashboard {
                        style: uiStyle
                        controller: backupSetController
                        launcher: backupLauncher
                        copies: recentBackupCopies
                        folderBrowser: protonFolderBrowser
                        restoreState: restoreController
                        windowHeight: root.height
                        onNewSetRequested: root.createNewSet()
                        onEditSetRequested: function(index) { root.editSet(index) }
                        onRemoveSetRequested: function(index) { root.requestRemoveSet(index) }
                        onRestoreRequested: function(index) { root.restoreRecentBackup(index) }
                        onOpenFolderRequested: function(index) { root.openRecentBackupFolder(index) }
                        onDetailsRequested: function(setId) {
                            backupDetailsDialog.setId = setId
                            backupDetailsDialog.open()
                        }
                    }

                    RestorePanel {
                        id: restorePanel
                        style: uiStyle
                        controller: restoreController
                        visible: root.showRestore
                        onCompleted: {
                            root.showRestore = false
                            dashboardScrollView.contentItem.contentY = 0
                        }
                    }
                }
            }

            BackupEditor {
                id: backupEditor
                style: uiStyle
                controller: backupSetController
                resources: resourceUsage
                onCloseRequested: root.showEditor = false
            }
        }
    }

    onActiveChanged: {
        if (active) {
            protonAuth.refresh()
            backupScheduler.refresh()
        }
    }

    Connections {
        target: backupScheduler
        function onFailed(error) { root.setStatus(error) }
    }

    Connections {
        target: resourceUsage
        function onStatusChanged(message) { root.setStatus(message) }
        function onFailed(error) { root.setStatus(error) }
    }

    Connections {
        target: themeColors
        function onColorsChanged() { Qt.callLater(root.updateDisabledPalette) }
    }

    Connections {
        target: protonAuth
        function onFailed(error) { root.setStatus(error) }
    }

    Connections {
        target: backupSetController
        function onStatusChanged(status) { root.setStatus(status) }
        function onFailed(error) { root.setStatus(error) }
    }

    Connections {
        target: backupLauncher
        function onStarted() { root.setStatus("") }
        function onFailed(error) { root.setStatus(error) }
    }

    Connections {
        target: recentBackupCopies
        function onFolderResolved(path) { protonFolderBrowser.openFolder(path) }
        function onDeleteConfirmationReady(name, path) {
            deleteCopyDialog.backupName = name
            deleteCopyDialog.copyPath = path
            deleteCopyDialog.open()
        }
        function onCopyDeleted(setId) { backupSetController.refreshRunState() }
        function onStatusChanged(message) { root.setStatus(message) }
        function onFailed(error) { root.setStatus(error) }
    }

    Connections {
        target: protonFolderBrowser
        function onFolderResolved(url) {
            if (!Qt.openUrlExternally(url)) {
                root.setStatus(qsTr("Unable to open Proton Drive in your browser."))
            }
        }
        function onFailed(error) { root.setStatus(error) }
    }

    Connections {
        target: restoreController
        function onStatusChanged(status) { root.setStatus(status) }
        function onFailed(error) { root.setStatus(error) }
    }
}
