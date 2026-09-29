import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 860
    height: 760
    title: qsTr("Custos Backup")
    property var selectedRestoreIndexes: []
    property bool syncingCurrentSet: false
    property bool showEditor: false
    property bool showRestore: false
    property bool backupRunning: backupSetController.currentRunStatus === "running"
    property string systemFontFamily: Qt.application.font.family
    property string displayFontFamily: systemFontFamily
    property string bodyFontFamily: systemFontFamily
    property int displayTypeSize: 34
    property int pageTitleSize: 24
    property int sectionTitleSize: 16
    property int bodyTypeSize: 14
    property int metadataTypeSize: 12
    property real bodyLeading: 1.4
    property int readableMeasure: 680
    property int contentPadding: 24
    property int cardPadding: 12
    property color accentColor: osPalette.window.hslLightness < 0.5 ? "#8AB4F8" : "#2457A6"

    SystemPalette {
        id: osPalette
        colorGroup: SystemPalette.Active
    }

    palette.window: osPalette.window
    palette.windowText: osPalette.windowText
    palette.base: osPalette.base
    palette.alternateBase: osPalette.alternateBase
    palette.text: osPalette.text
    palette.button: osPalette.button
    palette.buttonText: osPalette.buttonText
    palette.highlight: osPalette.highlight
    palette.highlightedText: osPalette.highlightedText
    palette.placeholderText: osPalette.placeholderText

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
        requiredMountsField.text = backupSetController.currentRequiredMounts.join("\n")
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
        setSelector.currentIndex = backupSetController.currentIndex
        loadCurrentSet()
        showEditor = true
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
            backupSetController.currentRequiredMounts = lines(requiredMountsField.text)
        } finally {
            syncingCurrentSet = false
        }
    }

    function setStatus(message) {
        statusLabel.text = message
        dashboardStatusLabel.text = message
    }

    Component.onCompleted: {
        showEditor = false
        loadCurrentSet()
    }

    Dialog {
        id: removeSetDialog
        property int setIndex: -1
        property string setName: ""
        title: qsTr("Remove backup set")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: Label {
            text: qsTr("Remove \"%1\" from Custos? Existing remote backups will not be deleted.").arg(removeSetDialog.setName)
            font.pixelSize: root.bodyTypeSize
            lineHeight: root.bodyLeading
            lineHeightMode: Text.ProportionalHeight
            wrapMode: Text.WordWrap
            Layout.preferredWidth: 360
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
            color: root.accentColor
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

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: qsTr("Dashboard")
                            font.family: root.displayFontFamily
                            font.pixelSize: root.pageTitleSize
                            font.weight: Font.Bold
                            color: root.accentColor
                            Layout.fillWidth: true
                        }

                        Button {
                            text: qsTr("New backup set")
                            onClicked: root.createNewSet()
                        }
                    }

                    GroupBox {
                        title: qsTr("Backup sets")
                        font.family: root.bodyFontFamily
                        font.pixelSize: root.sectionTitleSize
                        font.weight: Font.Bold
                        padding: root.cardPadding
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(84, Math.min(260, dashboardSetsList.contentHeight + 56))

                        ListView {
                            id: dashboardSetsList
                            anchors.fill: parent
                            model: backupSetController.setNames
                            clip: true

                            delegate: Frame {
                                required property int index
                                required property string modelData
                                width: dashboardSetsList.width
                                implicitHeight: 64

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: root.cardPadding

                                    Label {
                                        text: modelData
                                        font.family: root.bodyFontFamily
                                        font.pixelSize: root.bodyTypeSize
                                        font.weight: Font.Bold
                                        color: root.accentColor
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 60
                                    }

                                    Button {
                                        text: qsTr("Edit")
                                        font.pixelSize: root.metadataTypeSize
                                        Layout.preferredWidth: 52
                                        onClicked: {
                                            backupSetController.currentIndex = index
                                            loadCurrentSet()
                                            root.showEditor = true
                                        }
                                    }

                                    BusyIndicator {
                                        running: backupSetController.runningSetIds.indexOf(backupSetController.setIds[index]) >= 0
                                        visible: running
                                        Layout.preferredWidth: 24
                                        Layout.preferredHeight: 24
                                    }

                                    Button {
                                        text: qsTr("Back up")
                                        font.pixelSize: root.metadataTypeSize
                                        Layout.preferredWidth: 70
                                        enabled: index < backupSetController.setIds.length
                                        onClicked: backupLauncher.startBackup(backupSetController.setIds[index])
                                    }

                                    Button {
                                        text: qsTr("Remove set")
                                        font.pixelSize: root.metadataTypeSize
                                        Layout.preferredWidth: 88
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Remove this backup set")
                                        onClicked: root.requestRemoveSet(index)
                                    }
                                }
                            }
                        }
                    }

                    GroupBox {
                        title: qsTr("Recent backups")
                        font.family: root.bodyFontFamily
                        font.pixelSize: root.sectionTitleSize
                        font.weight: Font.Bold
                        padding: root.cardPadding
                        width: parent.width
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(110, Math.min(420, recentBackupsList.contentHeight + 64))

                        ColumnLayout {
                            anchors.fill: parent

                            Label {
                                text: qsTr("No backup runs yet. Create a set and run a preview to get started.")
                                font.pixelSize: root.bodyTypeSize
                                lineHeight: root.bodyLeading
                                lineHeightMode: Text.ProportionalHeight
                                visible: backupSetController.recentBackups.length === 0
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            ListView {
                                id: recentBackupsList
                                model: backupSetController.recentBackups
                                visible: backupSetController.recentBackups.length > 0
                                implicitWidth: 0
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true

                                delegate: Frame {
                                    required property int index
                                required property string modelData
                                width: recentBackupsList.width
                                implicitHeight: 76

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: root.cardPadding

                                    Label {
                                        id: summaryLabel
                                        text: modelData
                                            + (backupSetController.recentBackupTimestamps[index].length > 0
                                                ? qsTr("\nLast activity: %1").arg(backupSetController.recentBackupTimestamps[index])
                                                : "")
                                        wrapMode: Text.WordWrap
                                        lineHeight: root.bodyLeading
                                        lineHeightMode: Text.ProportionalHeight
                                        font.pixelSize: root.bodyTypeSize
                                        Layout.fillWidth: true
                                    }

                                        Button {
                                            text: qsTr("Restore")
                                            enabled: backupSetController.recentBackupTimestamps[index].length > 0
                                            onClicked: root.restoreRecentBackup(index)
                                        }
                                    }
                                }
                            }
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
                                placeholderText: qsTr("Search computer, set, copy, or status")
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

                            Label {
                                id: dashboardStatusLabel
                                font.pixelSize: root.metadataTypeSize
                                lineHeight: root.bodyLeading
                                lineHeightMode: Text.ProportionalHeight
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Label {
                        text: qsTr("Select a backup set above to edit sources, scheduling, and retention.")
                        font.pixelSize: root.metadataTypeSize
                        lineHeight: root.bodyLeading
                        lineHeightMode: Text.ProportionalHeight
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
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

                        Label {
                            text: qsTr("Backup set")
                            font.family: root.displayFontFamily
                            font.pixelSize: root.pageTitleSize
                            font.weight: Font.Bold
                            color: root.accentColor
                            Layout.fillWidth: true
                        }

                        Button {
                            text: qsTr("Close")
                            onClicked: root.showEditor = false
                        }
                    }

                    Label {
                        text: qsTr("Create independent sets with multiple sources and exclusions. Preview before saving or running a set.")
                        font.pixelSize: root.bodyTypeSize
                        lineHeight: root.bodyLeading
                        lineHeightMode: Text.ProportionalHeight
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        Layout.maximumWidth: root.readableMeasure
                    }

            RowLayout {
                Layout.fillWidth: true

                ComboBox {
                    id: setSelector
                    model: backupSetController.setNames
                    currentIndex: backupSetController.currentIndex
                    Layout.fillWidth: true
                    onCurrentIndexChanged: {
                        if (currentIndex >= 0 && currentIndex !== backupSetController.currentIndex) {
                            backupSetController.currentIndex = currentIndex
                            loadCurrentSet()
                        }
                    }
                }

                Button {
                    text: qsTr("Remove")
                    onClicked: {
                        backupSetController.removeCurrentSet()
                        setSelector.currentIndex = backupSetController.currentIndex
                        loadCurrentSet()
                    }
                }
            }

            TextField {
                id: setNameField
                placeholderText: qsTr("Set name")
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

            TextArea {
                id: exclusionsField
                placeholderText: qsTr("Excluded files or folders, one per line")
                wrapMode: TextArea.Wrap
                Layout.fillWidth: true
                Layout.preferredHeight: 72
            }

            Label {
                text: qsTr("Remote Proton Drive folder")
                font.pixelSize: root.sectionTitleSize
                font.weight: Font.DemiBold
                color: root.accentColor
            }

            Label {
                text: qsTr("This is where Custos creates backup copies inside Proton Drive, not a local folder. The final folder name, such as set-1, identifies this backup set and can be changed.")
                font.pixelSize: root.bodyTypeSize
                lineHeight: root.bodyLeading
                lineHeightMode: Text.ProportionalHeight
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.maximumWidth: root.readableMeasure
            }

            TextField {
                id: remoteField
                placeholderText: qsTr("Example: /my-files/backups/custos")
                Layout.fillWidth: true
            }

            Flow {
                width: parent.width
                spacing: 12

                Label {
                    text: qsTr("Retain successful copies")
                    font.pixelSize: root.bodyTypeSize
                }

                SpinBox {
                    id: retentionSpin
                    from: 1
                    to: 100
                    value: 3
                    editable: true
                    width: 90
                }
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

            CheckBox {
                id: acPowerCheck
                text: qsTr("Only back up on AC power")
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

            TextArea {
                id: requiredMountsField
                placeholderText: qsTr("Required external volume mount points, one per line")
                wrapMode: TextArea.Wrap
                Layout.fillWidth: true
                Layout.preferredHeight: 56
            }

            RowLayout {
                Layout.fillWidth: true

                Button {
                    text: qsTr("Save set")
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
                    enabled: backupSetController.previewIncluded.length > 0
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
