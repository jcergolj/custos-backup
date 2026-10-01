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
    title: qsTr("Custos Backup")
    property var selectedRestoreIndexes: []
    property bool syncingCurrentSet: false
    property bool showEditor: false
    property bool showRestore: false
    property bool showAdvanced: false
    property bool backupRunning: backupSetController.currentRunStatus === "running"
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
    readonly property color inkColor: "#19232e"
    readonly property color mutedColor: "#586575"
    readonly property color lineColor: "#dce1e7"
    readonly property color softColor: "#f0f3f6"
    readonly property color accentColor: "#245bcb"

    font.family: bodyFontFamily
    font.pixelSize: bodyTypeSize
    color: "#ffffff"
    palette {
        window: "#ffffff"
        windowText: root.inkColor
        base: "#ffffff"
        alternateBase: root.softColor
        text: root.inkColor
        button: "#ffffff"
        buttonText: root.inkColor
        brightText: "#ffffff"
        highlight: root.accentColor
        highlightedText: "#ffffff"
        placeholderText: root.mutedColor
        light: "#ffffff"
        midlight: root.softColor
        mid: root.lineColor
        dark: "#aeb7c2"
        shadow: root.lineColor
        toolTipBase: root.softColor
        toolTipText: root.inkColor
        accent: root.accentColor
        link: root.accentColor
        linkVisited: root.accentColor
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
        showRestore = true
        restoreController.discover(backupSetController.currentRemoteRoot)
    }

    function requestRemoveSet(index) {
        removeSetDialog.setIndex = index
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
        statusLabel.text = message
        dashboardStatusLabel.text = message
    }

    Component.onCompleted: {
        // Shared palette roles update every color group; apply disabled roles last.
        palette.disabled.text = mutedColor
        palette.disabled.windowText = mutedColor
        palette.disabled.buttonText = mutedColor
        palette.disabled.button = softColor
        showEditor = false
        loadCurrentSet()
    }

    Dialog {
        id: removeSetDialog
        objectName: "removeSetDialog"
        anchors.centerIn: parent
        property int setIndex: -1
        property string setName: ""
        title: qsTr("Remove backup")
        width: Math.min(root.width - 2 * root.contentPadding, 420)
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: Label {
            text: qsTr("Remove \"%1\" from Custos? Existing remote copies will not be deleted.").arg(removeSetDialog.setName)
            font.pixelSize: root.bodyTypeSize
            lineHeight: root.bodyLeading
            lineHeightMode: Text.ProportionalHeight
            wrapMode: Text.WordWrap
            padding: 16
        }

        onAccepted: {
            if (setIndex >= 0) {
                backupSetController.removeSet(setIndex)
            }
            setIndex = -1
            setName = ""
        }

        onRejected: {
            setIndex = -1
            setName = ""
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Label {
            text: qsTr("Custos Backup")
            font.family: root.displayFontFamily
            font.pixelSize: root.displayTypeSize
            font.weight: Font.Bold
            font.letterSpacing: 0.4
            color: root.inkColor
            Layout.leftMargin: root.contentPadding
            Layout.topMargin: 24
            Layout.bottomMargin: 16
        }

        StackLayout {
            currentIndex: root.showEditor ? 1 : 0
            Layout.fillWidth: true
            Layout.fillHeight: true

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

                    Label {
                        text: qsTr("Backups")
                        font.pixelSize: root.pageTitleSize
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56 + Math.max(180, Math.min(360,
                            root.height - 280,
                            Math.max(dashboardSetsList.contentHeight, recentBackupsList.contentHeight)))
                        spacing: 32

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredWidth: 1
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

                                Button {
                                    objectName: "newBackupSetButton"
                                    text: qsTr("New backup set")
                                    font.pixelSize: root.metadataTypeSize
                                    Layout.preferredHeight: 36
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
                                    implicitHeight: Math.max(80, setNameLabel.implicitHeight + 32)
                                    padding: 16
                                    background: Rectangle {
                                        color: "#ffffff"
                                        radius: 8
                                        border.color: root.lineColor
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: 12

                                        Label {
                                            id: setNameLabel
                                            text: setRow.modelData
                                            font.pixelSize: root.bodyTypeSize
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                        }

                                        BusyIndicator {
                                            objectName: "setBusy-" + setRow.index
                                            running: setRow.runActive
                                            visible: running
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                        }

                                        Label {
                                            text: qsTr("Running")
                                            visible: setRow.runActive
                                            font.pixelSize: root.metadataTypeSize
                                            color: root.mutedColor
                                        }

                                        ToolButton {
                                            id: overflowButton
                                            objectName: "setActions-" + setRow.index
                                            text: "⋯"
                                            font.pixelSize: 24
                                            Layout.preferredWidth: 40
                                            Layout.preferredHeight: 40
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
                            Layout.preferredWidth: 1
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
                                spacing: 10
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
                                    width: recentBackupsList.width
                                    implicitHeight: Math.max(96, recentSummary.implicitHeight + 32)
                                    padding: 16
                                    background: Rectangle {
                                        color: root.softColor
                                        radius: 8
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: 12

                                        ColumnLayout {
                                            id: recentSummary
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            Layout.alignment: Qt.AlignTop
                                            spacing: 4

                                            Label {
                                                objectName: "recentSummary-" + recentRow.index
                                                text: recentRow.modelData
                                                wrapMode: Text.Wrap
                                                lineHeight: root.bodyLeading
                                                lineHeightMode: Text.ProportionalHeight
                                                font.pixelSize: root.bodyTypeSize
                                                Layout.fillWidth: true
                                            }

                                            Label {
                                                text: qsTr("Last activity: %1").arg(recentRow.timestamp)
                                                visible: recentRow.timestamp.length > 0
                                                color: root.mutedColor
                                                font.pixelSize: root.metadataTypeSize
                                                wrapMode: Text.Wrap
                                                Layout.fillWidth: true
                                            }
                                        }

                                        Button {
                                            objectName: "restore-" + recentRow.index
                                            text: qsTr("Restore")
                                            Layout.preferredHeight: 36
                                            Layout.alignment: Qt.AlignTop
                                            enabled: recentRow.timestamp.length > 0
                                                && backupSetController.setIds.indexOf(backupSetController.recentBackupSetIds[recentRow.index]) >= 0
                                            onClicked: root.restoreRecentBackup(recentRow.index)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        id: dashboardStatusLabel
                        objectName: "dashboardStatusLabel"
                        visible: text.length > 0
                        font.pixelSize: root.bodyTypeSize
                        lineHeight: root.bodyLeading
                        lineHeightMode: Text.ProportionalHeight
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        padding: 12
                        background: Rectangle {
                            color: root.softColor
                            radius: 6
                        }
                    }

                    GroupBox {
                        title: qsTr("Restore")
                        font.family: root.bodyFontFamily
                        font.pixelSize: root.sectionTitleSize
                        font.weight: Font.Bold
                        padding: root.cardPadding
                        visible: root.showRestore
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(320, restoreContent.implicitHeight + 32)

                        ColumnLayout {
                            id: restoreContent
                            anchors.fill: parent
                            spacing: 16

                            Label {
                                text: qsTr("Choose a remote copy, then select files or a folder to restore.")
                                font.pixelSize: root.bodyTypeSize
                                lineHeight: root.bodyLeading
                                lineHeightMode: Text.ProportionalHeight
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            TextField {
                                placeholderText: qsTr("Search computer, backup name, copy, or status")
                                text: restoreController.copySearch
                                onTextChanged: restoreController.copySearch = text
                                Layout.fillWidth: true
                            }

                            ComboBox {
                                id: remoteCopySelector
                                model: restoreController.copies
                                Layout.fillWidth: true
                                onCurrentIndexChanged: restoreController.selectCopy(currentIndex)
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
                                text: qsTr("Restore from a manifest")
                                font.family: root.bodyFontFamily
                                font.pixelSize: root.sectionTitleSize
                                font.weight: Font.Bold
                                color: root.accentColor
                                Layout.fillWidth: true
                            }

                            ListView {
                                id: restoreList
                                model: restoreController.entries
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.min(180, contentHeight)
                                clip: true
                                delegate: CheckBox {
                                    required property int index
                                    required property string modelData
                                    text: modelData
                                    width: restoreList.width
                                    onCheckedChanged: {
                                        let selected = root.selectedRestoreIndexes.slice()
                                        const position = selected.indexOf(index)
                                        if (checked && position < 0) {
                                            selected.push(index)
                                        } else if (!checked && position >= 0) {
                                            selected.splice(position, 1)
                                        }
                                        root.selectedRestoreIndexes = selected
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true

                                TextField {
                                    id: destinationField
                                    text: restoreController.defaultDestination
                                    placeholderText: qsTr("Restore destination folder")
                                    Layout.fillWidth: true
                                }

                                Button {
                                    text: qsTr("Restore selected")
                                    enabled: root.selectedRestoreIndexes.length > 0 && destinationField.text.length > 0
                                    onClicked: restoreController.restoreSelected(root.selectedRestoreIndexes, destinationField.text)
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true

                                TextField {
                                    id: folderField
                                    placeholderText: qsTr("Optional folder path in selected copy")
                                    Layout.fillWidth: true
                                }

                                Button {
                                    text: qsTr("Restore folder")
                                    enabled: folderField.text.length > 0 && destinationField.text.length > 0
                                    onClicked: restoreController.restoreFolder(folderField.text, destinationField.text)
                                }
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

                        Button {
                            text: qsTr("Close")
                            onClicked: root.showEditor = false
                        }
                    }

                    Label {
                        text: qsTr("Give your backup a name, choose what to include or exclude, and set its schedule.")
                        font.pixelSize: root.bodyTypeSize
                        lineHeight: root.bodyLeading
                        lineHeightMode: Text.ProportionalHeight
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        Layout.maximumWidth: root.readableMeasure
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

                Button {
                    id: addSourceButton
                    text: "+"
                    font.pixelSize: 20
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

                    Button {
                        text: "-"
                        font.pixelSize: 20
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

                Button {
                    text: "+"
                    font.pixelSize: 20
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Exclude a file or folder")
                    onClicked: exclusionMenu.open()
                }
            }

            Menu {
                id: exclusionMenu

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
                placeholderText: qsTr("Choose exclusions with + or enter paths, one per line")
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

            RowLayout {
                Layout.fillWidth: true

                Button {
                    text: qsTr("Save")
                    onClicked: {
                        syncCurrentSet()
                        backupSetController.save()
                    }
                }

                Button {
                    text: qsTr("Preview")
                    onClicked: {
                        syncCurrentSet()
                        backupSetController.preview()
                    }
                }

                Button {
                    text: qsTr("Back up")
                    enabled: sourceModel.count > 0 && setNameField.text.trim().length > 0
                    onClicked: {
                        syncCurrentSet()
                        if (backupSetController.save()) {
                            root.setStatus(qsTr("Starting background backup..."))
                            backupLauncher.startBackup(backupSetController.currentId)
                        }
                    }
                }

                BusyIndicator {
                    running: root.backupRunning
                    visible: running
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                }

                Label {
                    text: qsTr("Backup in progress...")
                    font.pixelSize: root.metadataTypeSize
                    color: root.accentColor
                    visible: root.backupRunning
                }
            }

            Label {
                text: qsTr("Next run: %1").arg(backupSetController.currentNextRun)
                font.pixelSize: root.metadataTypeSize
            }

            Label {
                text: qsTr("Run state: %1%2")
                    .arg(backupSetController.currentRunStatus)
                    .arg(backupSetController.currentRunError.length > 0
                        ? qsTr(" (%1)").arg(backupSetController.currentRunError)
                        : "")
                Layout.fillWidth: true
                font.pixelSize: root.metadataTypeSize
                lineHeight: root.bodyLeading
                lineHeightMode: Text.ProportionalHeight
                wrapMode: Text.WordWrap
            }

            Button {
                text: qsTr("Advanced settings")
                checkable: true
                checked: root.showAdvanced
                onClicked: root.showAdvanced = checked
            }

            GroupBox {
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
                }
            }

            Label {
                id: statusLabel
                font.pixelSize: root.bodyTypeSize
                lineHeight: root.bodyLeading
                lineHeightMode: Text.ProportionalHeight
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            Label {
                text: qsTr("Included files (%1)").arg(backupSetController.previewIncluded.length)
                font.pixelSize: root.sectionTitleSize
                font.weight: Font.DemiBold
                color: root.accentColor
            }

            ListView {
                id: previewList
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
                text: qsTr("Excluded: %1, skipped: %2, missing: %3")
                    .arg(backupSetController.previewExcluded.length)
                    .arg(backupSetController.previewSkipped.length)
                    .arg(backupSetController.previewMissing.length)
                Layout.fillWidth: true
                font.pixelSize: root.metadataTypeSize
                lineHeight: root.bodyLeading
                lineHeightMode: Text.ProportionalHeight
                wrapMode: Text.WordWrap
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

            Button {
                text: qsTr("Confirm proposed cleanup")
                visible: backupSetController.cleanupConfirmationRequired
                onClicked: backupSetController.confirmCleanup()
            }

        }
    }

        }
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
        function onStarted() { root.setStatus(qsTr("Backup started.")) }
        function onFailed(error) { root.setStatus(error) }
    }

    Connections {
        target: restoreController
        function onEntriesChanged() { root.selectedRestoreIndexes = [] }
        function onCopiesChanged() { remoteCopySelector.currentIndex = -1 }
        function onStatusChanged(status) { root.setStatus(status) }
        function onFailed(error) { root.setStatus(error) }
    }
}
