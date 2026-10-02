import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 960
    height: 640
    minimumWidth: 760
    minimumHeight: 480
    title: qsTr("OmaCustos")
    property var selectedRestoreIndexes: []
    property var selectedRestorePaths: []
    property int selectedRestoreCopyIndex: -1
    property bool syncingCurrentSet: false
    property bool showEditor: false
    property bool showRestore: false
    property bool showAdvanced: false
    property bool backupRunning: backupSetController.currentRunStatus === "running"
    property string notificationMessage: ""
    property string systemFontFamily: Qt.application.font.family
    property string displayFontFamily: systemFontFamily
    property string bodyFontFamily: systemFontFamily
    property int displayTypeSize: 20
    property int pageTitleSize: 24
    property int sectionTitleSize: 16
    property int bodyTypeSize: 14
    property int metadataTypeSize: 12
    property real bodyLeading: 1.4
    property int readableMeasure: 680
    property int contentPadding: 24
    property int cardPadding: 12
    readonly property color backgroundColor: themeColors.colors.background
    readonly property color inkColor: themeColors.colors.foreground
    readonly property color mutedColor: themeColors.colors.muted
    readonly property color lineColor: themeColors.colors.border
    readonly property color softColor: themeColors.colors.surface
    readonly property color accentColor: themeColors.colors.accent

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

    component ActionButton: Button {
        font.family: root.bodyFontFamily
        font.pixelSize: root.bodyTypeSize
        font.weight: Font.Normal
        Layout.preferredHeight: 36
    }

    ListModel {
        id: sourceModel
    }

    function lines(value) {
        return value.split(/\r?\n/).map(function (line) {
            return line.trim()
        }).filter(function (line) {
            return line.length > 0
        })
    }

    function loadCurrentSet() {
        setNameField.text = backupSetController.currentName
        remoteField.text = backupSetController.currentRemoteRoot
        sourceModel.clear()
        backupSetController.currentSources.forEach(function (source) {
            sourceModel.append({ path: source })
        })
        exclusionsField.text = backupSetController.currentExclusions.join("\n")
        scheduleFrequency.currentIndex = scheduleFrequency.model.indexOf(backupSetController.currentScheduleFrequency)
        scheduleTimeField.text = "%1:%2".arg(backupSetController.currentScheduleHour.toString().padStart(2, "0"))
            .arg(backupSetController.currentScheduleMinute.toString().padStart(2, "0"))
        scheduleWeekday.currentIndex = backupSetController.currentScheduleWeekday - 1
        scheduleDay.value = backupSetController.currentScheduleDayOfMonth
        retentionSpin.value = backupSetController.currentRetention
        acPowerCheck.checked = backupSetController.currentOnlyOnAcPower
        systemResourceDefaults.checked = resourceUsage.presetIndex < 0
        resourcePreset.currentIndex = Math.max(0, resourceUsage.presetIndex)
    }

    function localPath(url) {
        return decodeURIComponent(url.toString().replace(/^file:\/\//, ""))
    }

    function addSource(url) {
        const path = localPath(url)
        if (path.length > 0) {
            sourceModel.append({ path: path })
        }
    }

    function addSources(urls) {
        urls.forEach(function (url) { addSource(url) })
    }

    function createNewSet() {
        backupSetController.addSet()
        loadCurrentSet()
        showAdvanced = false
        showEditor = true
    }

    function addExclusion(url) {
        const path = localPath(url)
        const exclusions = lines(exclusionsField.text)
        if (path.length > 0 && exclusions.indexOf(path) < 0) {
            exclusions.push(path)
            exclusionsField.text = exclusions.join("\n")
        }
    }

    function restoreRecentBackup(index) {
        const setId = backupSetController.recentBackupSetIds[index]
        const setIndex = backupSetController.setIds.indexOf(setId)
        if (setIndex < 0) {
            return
        }

        backupSetController.currentIndex = setIndex
        root.selectedRestoreIndexes = []
        root.selectedRestorePaths = []
        root.selectedRestoreCopyIndex = -1
        destinationField.text = ""
        showRestore = true
        restoreController.discover(backupSetController.recentBackupFolderPath(setId), setId)
        Qt.callLater(function () {
            restoreCopySearch.forceActiveFocus()
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

    function syncCurrentSet() {
        syncingCurrentSet = true
        try {
            backupSetController.currentName = setNameField.text
            backupSetController.currentRemoteRoot = remoteField.text
            const sources = []
            for (let index = 0; index < sourceModel.count; ++index) {
                const path = sourceModel.get(index).path.trim()
                if (path.length > 0) {
                    sources.push(path)
                }
            }
            backupSetController.currentSources = sources
            backupSetController.currentExclusions = lines(exclusionsField.text)
            backupSetController.currentScheduleFrequency = scheduleFrequency.currentText
            const timeParts = scheduleTimeField.text.split(":")
            backupSetController.currentScheduleHour = Number(timeParts[0])
            backupSetController.currentScheduleMinute = Number(timeParts[1])
            backupSetController.currentScheduleWeekday = scheduleWeekday.currentIndex + 1
            backupSetController.currentScheduleDayOfMonth = scheduleDay.value
            backupSetController.currentRetention = retentionSpin.value
            backupSetController.currentOnlyOnAcPower = acPowerCheck.checked
        } finally {
            syncingCurrentSet = false
        }
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
        loadCurrentSet()
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
                objectName: "importSetsButton"
                text: qsTr("Import")
                Layout.preferredHeight: 36
                enabled: !recentBackupCopies.busy
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Load backup sets from a JSON export")
                onClicked: importSetsDialog.open()
            }

            ActionButton {
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
                objectName: "protonSignInButton"
                text: qsTr("Sign in to Proton")
                visible: protonAuth.cliAvailable
                enabled: !protonAuth.checking
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Complete sign-in in your browser. Keep the terminal open until it finishes.")
                onClicked: protonAuth.signIn()
            }

            ActionButton {
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

                    RowLayout {
                        id: dashboardColumns
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56 + Math.max(180, Math.min(360,
                            root.height - 280,
                            Math.max(dashboardSetsList.contentHeight, recentBackupsList.contentHeight)))
                        spacing: 32

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredWidth: (dashboardColumns.width - dashboardColumns.spacing) / 3
                            Layout.minimumWidth: 0
                            spacing: 16

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 40

                                Label {
                                    objectName: "backupSetsTitle"
                                    text: qsTr("Backup sets")
                                    font.pixelSize: root.sectionTitleSize
                                    font.weight: Font.DemiBold
                                    Layout.fillWidth: true
                                }

                                ActionButton {
                                    objectName: "newBackupSetButton"
                                    text: "+"
                                    Layout.preferredWidth: 36
                                    Layout.preferredHeight: 36
                                    Accessible.name: qsTr("New backup set")
                                    ToolTip.visible: hovered
                                    ToolTip.text: Accessible.name
                                    onClicked: root.createNewSet()
                                }
                            }

                            ListView {
                                id: dashboardSetsList
                                objectName: "dashboardSetsList"
                                model: backupSetController.setNames
                                clip: true
                                spacing: 10
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                ScrollBar.vertical: ScrollBar {}

                                Label {
                                    objectName: "emptySetsLabel"
                                    text: qsTr("No backups yet")
                                    color: root.mutedColor
                                    visible: dashboardSetsList.count === 0
                                    width: parent.width
                                    padding: 16
                                }

                                delegate: Frame {
                                    id: setRow
                                    required property int index
                                    required property string modelData
                                    readonly property bool runActive: backupSetController.runningSetIds.indexOf(backupSetController.setIds[index]) >= 0
                                    width: dashboardSetsList.width
                                    implicitHeight: Math.max(80, setSummary.implicitHeight + 32)
                                    padding: 16
                                    background: Rectangle {
                                        color: root.backgroundColor
                                        radius: 8
                                        border.color: root.lineColor
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: 12

                                        ColumnLayout {
                                            id: setSummary
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            spacing: 4

                                            Label {
                                                id: setNameLabel
                                                text: setRow.modelData
                                                font.pixelSize: root.bodyTypeSize
                                                font.weight: Font.DemiBold
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                            }

                                            Label {
                                                objectName: "setRemainingTime-" + setRow.index
                                                text: backupSetController.remainingTimes[backupSetController.setIds[setRow.index]]
                                                    || qsTr("Estimating time remaining…")
                                                visible: setRow.runActive
                                                font.pixelSize: root.metadataTypeSize
                                                color: root.mutedColor
                                                wrapMode: Text.WordWrap
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                            }

                                            Label {
                                                objectName: "setTransferProgress-" + setRow.index
                                                text: (backupSetController.transferProgress[backupSetController.setIds[setRow.index]] || {}).text || ""
                                                visible: setRow.runActive && text.length > 0
                                                textFormat: Text.PlainText
                                                font.pixelSize: root.metadataTypeSize
                                                wrapMode: Text.WrapAnywhere
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                            }

                                            ProgressBar {
                                                objectName: "setProgressBar-" + setRow.index
                                                readonly property var progress: backupSetController.transferProgress[backupSetController.setIds[setRow.index]] || ({})
                                                visible: setRow.runActive
                                                value: progress.fraction || 0
                                                indeterminate: progress.indeterminate === undefined || progress.indeterminate
                                                Accessible.name: qsTr("Backup work processed")
                                                Layout.fillWidth: true
                                            }
                                        }

                                        BusyIndicator {
                                            objectName: "setBusy-" + setRow.index
                                            running: setRow.runActive
                                            visible: running
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                        }

                                        ActionButton {
                                            id: overflowButton
                                            objectName: "setActions-" + setRow.index
                                            text: "⋯"
                                            Layout.preferredWidth: 40
                                            Accessible.name: qsTr("Actions for %1").arg(setRow.modelData)
                                            ToolTip.visible: hovered
                                            ToolTip.text: Accessible.name
                                            onClicked: setActionsMenu.open()

                                            Menu {
                                                id: setActionsMenu
                                                objectName: "setMenu-" + setRow.index
                                                x: overflowButton.width - width
                                                y: overflowButton.height

                                                MenuItem {
                                                    text: qsTr("Edit")
                                                    onTriggered: {
                                                        backupSetController.currentIndex = setRow.index
                                                        root.loadCurrentSet()
                                                        root.showAdvanced = false
                                                        root.showEditor = true
                                                        Qt.callLater(function () { setNameField.forceActiveFocus() })
                                                    }
                                                }

                                                MenuItem {
                                                    text: qsTr("Back up now")
                                                    enabled: setRow.index < backupSetController.setIds.length && !setRow.runActive
                                                    onTriggered: backupLauncher.startBackup(backupSetController.setIds[setRow.index])
                                                }

                                                MenuSeparator {}

                                                MenuItem {
                                                    text: qsTr("Delete")
                                                    onTriggered: root.requestRemoveSet(setRow.index)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredWidth: 2 * (dashboardColumns.width - dashboardColumns.spacing) / 3
                            Layout.minimumWidth: 0
                            spacing: 16

                            Label {
                                objectName: "recentBackupsTitle"
                                text: qsTr("Recent backups")
                                font.pixelSize: root.sectionTitleSize
                                font.weight: Font.DemiBold
                                Layout.fillWidth: true
                                Layout.preferredHeight: 40
                                verticalAlignment: Text.AlignVCenter
                            }

                            ListView {
                                id: recentBackupsList
                                objectName: "recentBackupsList"
                                model: backupSetController.recentBackups
                                implicitWidth: 0
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 12
                                ScrollBar.vertical: ScrollBar {}

                                Label {
                                    objectName: "emptyRecentLabel"
                                    text: qsTr("No backups yet")
                                    color: root.mutedColor
                                    visible: recentBackupsList.count === 0
                                    width: parent.width
                                    padding: 16
                                }

                                delegate: Frame {
                                    id: recentRow
                                    required property int index
                                    required property string modelData
                                    readonly property string timestamp: backupSetController.recentBackupTimestamps[index] || ""
                                    readonly property var details: backupSetController.runDetails[backupSetController.recentBackupSetIds[index]] || ({})
                                    width: recentBackupsList.width
                                    implicitHeight: Math.max(80, recentText.implicitHeight + 40)
                                    padding: 20
                                    background: Rectangle {
                                        color: root.softColor
                                        radius: 8
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: 16

                                        ColumnLayout {
                                            id: recentText
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            spacing: 4
                                            Label {
                                                id: recentSummary
                                                objectName: "recentSummary-" + recentRow.index
                                                text: recentRow.modelData.replace(/\s*\r?\n\s*/g, " · ")
                                                    + (recentRow.timestamp.length > 0 ? " · " + recentRow.timestamp : "")
                                                textFormat: Text.PlainText
                                                font.pixelSize: root.bodyTypeSize
                                                wrapMode: Text.NoWrap
                                                elide: Text.ElideRight
                                                maximumLineCount: 1
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                ToolTip.visible: recentSummaryHover.hovered && truncated
                                                ToolTip.text: text

                                                HoverHandler { id: recentSummaryHover }
                                            }
                                            Label {
                                                objectName: "recentResultSummary-" + recentRow.index
                                                text: recentRow.details.summary || ""
                                                visible: text.length > 0
                                                textFormat: Text.PlainText
                                                font.pixelSize: root.metadataTypeSize
                                                wrapMode: Text.WordWrap
                                                Layout.fillWidth: true
                                            }
                                        }

                                        ActionButton {
                                            objectName: "openFolder-" + recentRow.index
                                            text: "↗"
                                            Layout.preferredWidth: 36
                                            Layout.preferredHeight: 36
                                            Accessible.name: qsTr("Open %1 in Proton Drive")
                                                .arg(backupSetController.recentBackups[recentRow.index].split("\n")[0])
                                            ToolTip.visible: hovered
                                            ToolTip.text: Accessible.name
                                            enabled: recentRow.timestamp.length > 0 && !protonFolderBrowser.busy && !recentBackupCopies.busy
                                                && backupSetController.setIds.indexOf(backupSetController.recentBackupSetIds[recentRow.index]) >= 0
                                            onClicked: root.openRecentBackupFolder(recentRow.index)
                                        }

                                        ActionButton {
                                            objectName: "restore-" + recentRow.index
                                            text: qsTr("Restore")
                                            Layout.preferredHeight: 36
                                            enabled: recentRow.timestamp.length > 0 && !restoreController.busy
                                                && backupSetController.setIds.indexOf(backupSetController.recentBackupSetIds[recentRow.index]) >= 0
                                            onClicked: root.restoreRecentBackup(recentRow.index)
                                        }

                                        ActionButton {
                                            id: recentActionsButton
                                            objectName: "recentActions-" + recentRow.index
                                            text: "⋯"
                                            Layout.preferredWidth: 36
                                            Layout.preferredHeight: 36
                                            Accessible.name: qsTr("Recent backup actions")
                                            onClicked: recentActionsMenu.open()
                                            Menu {
                                                id: recentActionsMenu
                                                objectName: "recentMenu-" + recentRow.index
                                                x: recentActionsButton.width - width
                                                y: recentActionsButton.height
                                                MenuItem {
                                                    text: qsTr("Delete copy")
                                                    enabled: recentRow.timestamp.length > 0 && !recentBackupCopies.busy
                                                        && backupSetController.setIds.indexOf(backupSetController.recentBackupSetIds[recentRow.index]) >= 0
                                                        && backupSetController.runningSetIds.indexOf(backupSetController.recentBackupSetIds[recentRow.index]) < 0
                                                    onTriggered: recentBackupCopies.requestDelete(backupSetController.recentBackupSetIds[recentRow.index])
                                                }
                                                MenuItem {
                                                    objectName: "viewBackupDetails-" + recentRow.index
                                                    text: qsTr("View details")
                                                    enabled: (recentRow.details.status || "").length > 0
                                                    onTriggered: {
                                                        backupDetailsDialog.setId = backupSetController.recentBackupSetIds[recentRow.index]
                                                        backupDetailsDialog.open()
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    GroupBox {
                        id: restorePanel
                        objectName: "restorePanel"
                        title: qsTr("Restore")
                        font.family: root.bodyFontFamily
                        font.pixelSize: root.sectionTitleSize
                        font.weight: Font.Bold
                        padding: root.cardPadding
                        visible: root.showRestore
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(320, restoreContent.implicitHeight + 32)
                        Layout.bottomMargin: root.contentPadding

                        ColumnLayout {
                            id: restoreContent
                            anchors.fill: parent
                            spacing: 16

                            Label {
                                objectName: "restoreInstructions"
                                text: qsTr("Choose a backup copy, tick the files you want to restore, and choose the folder to restore them to. Then press Start restore below.")
                                font.pixelSize: root.bodyTypeSize
                                lineHeight: root.bodyLeading
                                lineHeightMode: Text.ProportionalHeight
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            TextField {
                                id: restoreCopySearch
                                objectName: "restoreCopySearch"
                                placeholderText: qsTr("Search computer, backup name, copy, or status")
                                text: restoreController.copySearch
                                enabled: !restoreController.busy
                                onTextChanged: restoreController.copySearch = text
                                Layout.fillWidth: true
                            }

                            ComboBox {
                                id: remoteCopySelector
                                objectName: "restoreCopySelector"
                                model: restoreController.copies
                                currentIndex: restoreController.currentCopyIndex
                                enabled: !restoreController.busy && count > 0
                                Layout.fillWidth: true
                                onModelChanged: {
                                    if (restoreController.currentCopyIndex < 0) {
                                        currentIndex = -1
                                    }
                                }
                                onActivated: {
                                    if (root.selectedRestoreCopyIndex !== currentIndex) {
                                        root.selectedRestoreIndexes = []
                                        root.selectedRestorePaths = []
                                    }
                                    root.selectedRestoreCopyIndex = currentIndex
                                    restoreController.selectCopy(currentIndex)
                                }
                                displayText: currentIndex < 0 ? qsTr("Choose a backup copy…") : currentText
                            }

                             RowLayout {
                                 visible: restoreController.busy
                                Layout.fillWidth: true

                                BusyIndicator {
                                    objectName: "restoreLoadingIndicator"
                                    running: restoreController.busy
                                    Layout.preferredWidth: 28
                                    Layout.preferredHeight: 28
                                }

                                Label {
                                    objectName: "restoreLoadingMessage"
                                    text: restoreController.loadingMessage
                                    color: root.mutedColor
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                 }
                             }

                             Label {
                                 objectName: "restoreCachedDataMessage"
                                 text: restoreController.busy
                                     ? qsTr("Showing the last successful result while refreshing. Cached files cannot be restored until verification finishes.")
                                     : qsTr("Showing cached data from the last successful refresh. Select the copy to verify it again.")
                                 visible: restoreController.showingCachedData
                                 color: root.mutedColor
                                 wrapMode: Text.WordWrap
                                 Layout.fillWidth: true
                             }

                             Label {
                                text: qsTr("Unavailable or failed items: %1").arg(restoreController.unavailableEntries.length)
                                font.pixelSize: root.metadataTypeSize
                                visible: restoreController.unavailableEntries.length > 0
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }

                            ListView {
                                model: restoreController.unavailableEntries
                                visible: restoreController.unavailableEntries.length > 0
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.min(100, contentHeight)
                                clip: true
                                delegate: Label {
                                    required property string modelData
                                    text: modelData
                                    width: parent ? parent.width : 0
                                    elide: Text.ElideMiddle
                                }
                            }

                            Label {
                                text: qsTr("1. Tick the files to restore")
                                font.family: root.bodyFontFamily
                                font.pixelSize: root.sectionTitleSize
                                font.weight: Font.Bold
                                color: root.accentColor
                                Layout.fillWidth: true
                            }

                            ListView {
                                id: restoreList
                                objectName: "restoreFilesList"
                                model: restoreController.entries
                                Layout.fillWidth: true
                                Layout.preferredHeight: count > 0 ? Math.max(48, Math.min(180, contentHeight)) : 0
                                clip: true
                                delegate: CheckBox {
                                    required property int index
                                    required property string modelData
                                    objectName: "restoreFile-" + index
                                    text: modelData
                                    width: restoreList.width
                                    checked: root.selectedRestoreIndexes.indexOf(index) >= 0
                                    onToggled: {
                                        let selected = root.selectedRestoreIndexes.slice()
                                        const position = selected.indexOf(index)
                                        if (checked && position < 0) {
                                            selected.push(index)
                                        } else if (!checked && position >= 0) {
                                            selected.splice(position, 1)
                                        }
                                        root.selectedRestoreIndexes = selected
                                        root.selectedRestorePaths = selected.map(function (selectedIndex) {
                                            return restoreController.entries[selectedIndex]
                                        })
                                    }
                                }
                            }

                            Label {
                                text: remoteCopySelector.currentIndex < 0
                                    ? qsTr("Choose a backup copy to see the files available to restore.")
                                    : qsTr("This copy has no verified files available to restore.")
                                visible: restoreList.count === 0 && !restoreController.busy
                                color: root.mutedColor
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            Label {
                                objectName: "restoreSelectionCount"
                                text: qsTr("Selected files: %1").arg(root.selectedRestoreIndexes.length)
                                color: root.mutedColor
                                Layout.fillWidth: true
                            }

                            Label {
                                text: qsTr("2. Choose the destination folder")
                                font.pixelSize: root.sectionTitleSize
                                font.weight: Font.Bold
                                color: root.accentColor
                                Layout.fillWidth: true
                            }

                            RowLayout {
                                Layout.fillWidth: true

                                TextField {
                                    id: destinationField
                                    objectName: "restoreDestinationField"
                                    placeholderText: qsTr("Choose a folder or enter its full path")
                                    Accessible.name: qsTr("Restore destination folder")
                                    Layout.fillWidth: true
                                }

                                ActionButton {
                                    objectName: "chooseRestoreDestinationButton"
                                    text: qsTr("Choose folder…")
                                    onClicked: restoreDestinationDialog.open()
                                }
                            }

                            FolderDialog {
                                id: restoreDestinationDialog
                                objectName: "restoreDestinationDialog"
                                title: qsTr("Choose where to restore the selected files")
                                onAccepted: destinationField.text = root.localPath(selectedFolder)
                            }

                            Label {
                                text: qsTr("The selected files will be restored into this folder, keeping their backed-up folder structure.")
                                font.pixelSize: root.metadataTypeSize
                                color: root.mutedColor
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            ActionButton {
                                objectName: "startRestoreButton"
                                text: qsTr("Start restore")
                                Layout.alignment: Qt.AlignRight
                                enabled: restoreController.restoreEligible && root.selectedRestoreIndexes.length > 0 && destinationField.text.trim().length > 0
                                onClicked: restoreController.restoreSelected(root.selectedRestoreIndexes, destinationField.text.trim())
                            }

                        }
                    }

                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: root.contentPadding
                    anchors.rightMargin: root.contentPadding
                    spacing: 20

                    RowLayout {
                        Layout.fillWidth: true

                        Item { Layout.fillWidth: true }

                        ActionButton {
                            objectName: "closeEditorButton"
                            text: "×"
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            Accessible.name: qsTr("Close editor")
                            ToolTip.visible: hovered
                            ToolTip.text: Accessible.name
                            onClicked: root.showEditor = false
                        }
                    }

            Label {
                text: qsTr("Name")
                font.pixelSize: root.sectionTitleSize
                font.weight: Font.DemiBold
                color: root.accentColor
            }

            TextField {
                id: setNameField
                objectName: "setNameField"
                placeholderText: qsTr("Example: Documents")
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: qsTr("Source files and folders")
                    font.pixelSize: root.sectionTitleSize
                    font.weight: Font.DemiBold
                    color: root.accentColor
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                }

                ActionButton {
                    id: addSourceButton
                    objectName: "addSourceButton"
                    text: "+"
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Add a source file or folder")
                    onClicked: sourceMenu.open()
                }
            }

            Label {
                text: qsTr("Choose files or folders to include in this backup.")
                font.pixelSize: root.metadataTypeSize
                lineHeight: root.bodyLeading
                lineHeightMode: Text.ProportionalHeight
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            FileDialog {
                id: sourceFilesDialog
                title: qsTr("Select source files")
                fileMode: FileDialog.OpenFiles
                onAccepted: root.addSources(selectedFiles)
            }

            FolderDialog {
                id: sourceFolderDialog
                title: qsTr("Select source folder")
                onAccepted: root.addSource(selectedFolder)
            }

            Menu {
                id: sourceMenu
                objectName: "sourceMenu"
                parent: addSourceButton
                x: addSourceButton.width - width
                y: addSourceButton.height

                MenuItem {
                    text: qsTr("Add files")
                    onTriggered: sourceFilesDialog.open()
                }

                MenuItem {
                    text: qsTr("Add folder")
                    onTriggered: sourceFolderDialog.open()
                }
            }

            Label {
                text: qsTr("No source files or folders selected yet.")
                font.pixelSize: root.metadataTypeSize
                visible: sourceModel.count === 0
                Layout.fillWidth: true
            }

            ListView {
                id: sourceList
                model: sourceModel
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(48, Math.min(180, contentHeight))
                clip: true

                delegate: RowLayout {
                    required property int index
                    required property string path
                    width: sourceList.width

                    TextField {
                        text: path
                        placeholderText: qsTr("Source path")
                        Layout.fillWidth: true
                        onTextChanged: {
                            if (index >= 0 && index < sourceModel.count && sourceModel.get(index).path !== text) {
                                sourceModel.setProperty(index, "path", text)
                            }
                        }
                    }

                    ActionButton {
                        text: "-"
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Remove this source")
                        onClicked: sourceModel.remove(index)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: qsTr("Exclusions (optional)")
                    font.pixelSize: root.sectionTitleSize
                    font.weight: Font.DemiBold
                    color: root.accentColor
                    Layout.fillWidth: true
                }

                ActionButton {
                    id: addExclusionButton
                    objectName: "addExclusionButton"
                    text: "+"
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Exclude a file or folder")
                    onClicked: exclusionMenu.open()
                }
            }

            Menu {
                id: exclusionMenu
                objectName: "exclusionMenu"
                parent: addExclusionButton
                x: addExclusionButton.width - width
                y: addExclusionButton.height

                MenuItem {
                    text: qsTr("Exclude files")
                    onTriggered: exclusionFilesDialog.open()
                }

                MenuItem {
                    text: qsTr("Exclude folder")
                    onTriggered: exclusionFolderDialog.open()
                }
            }

            FileDialog {
                id: exclusionFilesDialog
                title: qsTr("Select files to exclude")
                fileMode: FileDialog.OpenFiles
                onAccepted: selectedFiles.forEach(function (url) { root.addExclusion(url) })
            }

            FolderDialog {
                id: exclusionFolderDialog
                title: qsTr("Select folder to exclude")
                onAccepted: root.addExclusion(selectedFolder)
            }

            TextArea {
                id: exclusionsField
                placeholderText: qsTr("Full paths or folder names, one per line (e.g. node_modules)")
                wrapMode: TextArea.Wrap
                Layout.fillWidth: true
                Layout.preferredHeight: 72
            }

            Flow {
                width: parent.width
                spacing: 12

                Label {
                    text: qsTr("Schedule")
                    font.pixelSize: root.bodyTypeSize
                }

                ComboBox {
                    id: scheduleFrequency
                    objectName: "scheduleFrequency"
                    model: ["disabled", "daily", "weekly", "monthly"]
                    width: 130
                }

                TextField {
                    id: scheduleTimeField
                    text: "02:00"
                    placeholderText: qsTr("HH:MM")
                    width: 90
                    visible: scheduleFrequency.currentText !== "disabled"
                }
            }

            Flow {
                visible: scheduleFrequency.currentText === "weekly"
                width: parent.width
                spacing: 12

                Label {
                    text: qsTr("Run every week on:")
                    font.pixelSize: root.bodyTypeSize
                }

                ComboBox {
                    id: scheduleWeekday
                    model: [qsTr("Monday"), qsTr("Tuesday"), qsTr("Wednesday"), qsTr("Thursday"), qsTr("Friday"), qsTr("Saturday"), qsTr("Sunday")]
                    width: 120
                }
            }

            Flow {
                visible: scheduleFrequency.currentText === "monthly"
                width: parent.width
                spacing: 12

                Label {
                    text: qsTr("Run every month on day:")
                    font.pixelSize: root.bodyTypeSize
                }

                SpinBox {
                    id: scheduleDay
                    from: 1
                    to: 31
                    value: 1
                    editable: true
                    width: 70
                }
            }

            Label {
                text: scheduleFrequency.currentText === "disabled"
                    ? qsTr("Scheduling is disabled.")
                    : scheduleFrequency.currentText === "daily"
                        ? qsTr("This backup runs every day at the selected time.")
                        : scheduleFrequency.currentText === "weekly"
                            ? qsTr("This backup runs every week at the selected time and weekday.")
                            : qsTr("This backup runs every month at the selected time. Days 29-31 use the last day when a month is shorter.")
                wrapMode: Text.WordWrap
                font.pixelSize: root.metadataTypeSize
                lineHeight: root.bodyLeading
                lineHeightMode: Text.ProportionalHeight
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Next run: %1").arg(backupSetController.currentNextRun)
                font.pixelSize: root.metadataTypeSize
            }

            ActionButton {
                objectName: "advancedSettingsButton"
                text: qsTr("Advanced settings")
                checkable: true
                checked: root.showAdvanced
                onClicked: root.showAdvanced = checked
            }

            GroupBox {
                objectName: "advancedSettingsPanel"
                visible: root.showAdvanced
                title: qsTr("Advanced settings")
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    Label {
                        text: qsTr("Remote Proton Drive folder")
                        font.pixelSize: root.sectionTitleSize
                        font.weight: Font.DemiBold
                        color: root.accentColor
                    }

                    Label {
                        text: qsTr("Copies are saved in Proton Drive under this folder, grouped by computer and backup name.")
                        font.pixelSize: root.bodyTypeSize
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    TextField {
                        id: remoteField
                        placeholderText: qsTr("Example: /my-files/backups")
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Label {
                            text: qsTr("Successful copies to keep")
                            font.pixelSize: root.bodyTypeSize
                        }

                        SpinBox {
                            id: retentionSpin
                            from: 1
                            to: 100
                            value: 3
                            editable: true
                        }
                    }

                    CheckBox {
                        id: acPowerCheck
                        text: qsTr("Only back up on AC power")
                    }

                    Label {
                        text: qsTr("Resource usage (all backups)")
                        font.pixelSize: root.sectionTitleSize
                        font.weight: Font.DemiBold
                        color: root.accentColor
                    }

                    CheckBox {
                        id: systemResourceDefaults
                        objectName: "useSystemResourceDefaults"
                        text: qsTr("Use system defaults")
                        enabled: !resourceUsage.busy
                    }

                    ComboBox {
                        id: resourcePreset
                        objectName: "resourceUsagePreset"
                        model: resourceUsage.names
                        enabled: !resourceUsage.busy && !systemResourceDefaults.checked
                        Layout.fillWidth: true
                        Accessible.name: qsTr("Backup resource usage")
                    }

                    Label {
                        objectName: "resourceUsageDescription"
                        text: systemResourceDefaults.checked
                            ? qsTr("No CPU cap · Normal CPU priority · Normal I/O scheduling")
                            : resourceUsage.descriptions[resourcePreset.currentIndex] || ""
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("Applies to all manual and scheduled backups after Save, when the next worker starts. 100% allows one full CPU core; 200% allows two. A lower nice value gives the worker higher CPU priority.")
                        font.pixelSize: root.metadataTypeSize
                        color: root.mutedColor
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            ListView {
                id: previewList
                visible: count > 0
                model: backupSetController.previewIncluded
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(150, contentHeight)
                clip: true
                delegate: Label {
                    required property string modelData
                    text: modelData
                    font.pixelSize: root.metadataTypeSize
                    elide: Text.ElideMiddle
                    width: previewList.width
                }
            }

            Label {
                text: qsTr("Retention cleanup is waiting for confirmation. Proposed deletions:")
                font.pixelSize: root.bodyTypeSize
                lineHeight: root.bodyLeading
                lineHeightMode: Text.ProportionalHeight
                visible: backupSetController.cleanupConfirmationRequired
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            ListView {
                model: backupSetController.cleanupTargets
                visible: backupSetController.cleanupConfirmationRequired
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(100, contentHeight)
                clip: true
                delegate: Label {
                    required property string modelData
                    text: modelData
                    font.pixelSize: root.metadataTypeSize
                    width: parent ? parent.width : 0
                    elide: Text.ElideMiddle
                }
            }

            ActionButton {
                text: qsTr("Confirm proposed cleanup")
                visible: backupSetController.cleanupConfirmationRequired
                onClicked: backupSetController.confirmCleanup()
            }

            RowLayout {
                objectName: "editorActionsRow"
                Layout.fillWidth: true
                Layout.bottomMargin: root.contentPadding

                ActionButton {
                    objectName: "saveBackupSetButton"
                    text: qsTr("Save")
                    enabled: !resourceUsage.busy
                    onClicked: {
                        syncCurrentSet()
                        if (backupSetController.save()) {
                            resourceUsage.save(systemResourceDefaults.checked ? -1 : resourcePreset.currentIndex)
                        }
                    }
                }

                ActionButton {
                    objectName: "previewBackupSetButton"
                    text: qsTr("Preview")
                    onClicked: {
                        syncCurrentSet()
                        backupSetController.preview()
                    }
                }

                BusyIndicator {
                    running: root.backupRunning
                    visible: running
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                }

                Label {
                    text: qsTr("Backup in progress… %1").arg(
                        backupSetController.remainingTimes[backupSetController.currentId]
                            || qsTr("Estimating time remaining…"))
                    font.pixelSize: root.metadataTypeSize
                    color: root.accentColor
                    visible: root.backupRunning
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            Label {
                objectName: "editorTransferProgress"
                text: (backupSetController.transferProgress[backupSetController.currentId] || {}).text || ""
                visible: root.backupRunning && text.length > 0
                textFormat: Text.PlainText
                font.pixelSize: root.metadataTypeSize
                wrapMode: Text.WrapAnywhere
                Layout.fillWidth: true
            }

        }
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
        function onPresetChanged() {
            systemResourceDefaults.checked = resourceUsage.presetIndex < 0
            resourcePreset.currentIndex = Math.max(0, resourceUsage.presetIndex)
        }
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
        function onCurrentSetChanged() {
            if (!root.syncingCurrentSet) {
                loadCurrentSet()
            }
        }
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
        function onRestoreCompleted() {
            root.showRestore = false
            root.selectedRestoreIndexes = []
            root.selectedRestorePaths = []
            root.selectedRestoreCopyIndex = -1
            destinationField.text = ""
            dashboardScrollView.contentItem.contentY = 0
        }
        function onEntriesChanged() {
            const paths = root.selectedRestorePaths.length > 0
                ? root.selectedRestorePaths : root.selectedRestoreIndexes.map(function (index) {
                    return restoreController.entries[index]
                })
            if (restoreController.entries.length === 0 && restoreController.busy) {
                return
            }
            root.selectedRestoreIndexes = paths.map(function (path) {
                return restoreController.entries.indexOf(path)
            }).filter(function (index) { return index >= 0 })
            root.selectedRestorePaths = root.selectedRestoreIndexes.map(function (index) {
                return restoreController.entries[index]
            })
        }
        function onCurrentCopyIndexChanged() {
            if (root.selectedRestoreCopyIndex !== restoreController.currentCopyIndex) {
                root.selectedRestoreIndexes = []
                root.selectedRestorePaths = []
                root.selectedRestoreCopyIndex = restoreController.currentCopyIndex
            }
        }
        function onStatusChanged(status) { root.setStatus(status) }
        function onFailed(error) { root.setStatus(error) }
    }
}
