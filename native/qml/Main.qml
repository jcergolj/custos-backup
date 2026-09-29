import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 860
    height: 760
    title: qsTr("Custos Backup")
    property var selectedRestoreIndexes: []

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
        sourcesField.text = backupSetController.currentSources.join("\n")
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

    function syncCurrentSet() {
        backupSetController.currentName = setNameField.text
        backupSetController.currentRemoteRoot = remoteField.text
        backupSetController.currentSources = lines(sourcesField.text)
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
    }

    Component.onCompleted: loadCurrentSet()

    ScrollView {
        anchors.fill: parent

        ColumnLayout {
            width: Math.min(parent.width - 48, 760)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12

            Label {
                text: qsTr("Backup sets")
                font.pixelSize: 28
            }

            Label {
                text: qsTr("Create independent sets with multiple sources and exclusions. Preview before saving or running a set.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
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
                    text: qsTr("New set")
                    onClicked: {
                        backupSetController.addSet()
                        setSelector.currentIndex = backupSetController.currentIndex
                        loadCurrentSet()
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

            TextArea {
                id: sourcesField
                placeholderText: qsTr("Source files or folders, one per line")
                wrapMode: TextArea.Wrap
                Layout.fillWidth: true
                Layout.preferredHeight: 72
            }

            TextArea {
                id: exclusionsField
                placeholderText: qsTr("Excluded files or folders, one per line")
                wrapMode: TextArea.Wrap
                Layout.fillWidth: true
                Layout.preferredHeight: 72
            }

            TextField {
                id: remoteField
                placeholderText: qsTr("Remote backup folder")
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true

                Label { text: qsTr("Retain successful copies") }

                SpinBox {
                    id: retentionSpin
                    from: 1
                    to: 100
                    value: 3
                    editable: true
                    Layout.preferredWidth: 90
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Label { text: qsTr("Schedule") }

                ComboBox {
                    id: scheduleFrequency
                    model: ["disabled", "daily", "weekly", "monthly"]
                    Layout.preferredWidth: 130
                }

                TextField {
                    id: scheduleTimeField
                    text: "02:00"
                    placeholderText: qsTr("HH:MM")
                    Layout.preferredWidth: 90
                }

                ComboBox {
                    id: scheduleWeekday
                    model: [qsTr("Monday"), qsTr("Tuesday"), qsTr("Wednesday"), qsTr("Thursday"), qsTr("Friday"), qsTr("Saturday"), qsTr("Sunday")]
                    Layout.preferredWidth: 120
                }

                SpinBox {
                    id: scheduleDay
                    from: 1
                    to: 31
                    value: 1
                    editable: true
                    Layout.preferredWidth: 70
                }
            }

            CheckBox {
                id: acPowerCheck
                text: qsTr("Only back up on AC power")
            }

            Label {
                text: qsTr("Next run: %1").arg(backupSetController.currentNextRun)
            }

            Label {
                text: qsTr("Run state: %1%2")
                    .arg(backupSetController.currentRunStatus)
                    .arg(backupSetController.currentRunError.length > 0
                        ? qsTr(" (%1)").arg(backupSetController.currentRunError)
                        : "")
                Layout.fillWidth: true
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
                            statusLabel.text = qsTr("Starting background backup...")
                            backupLauncher.startBackup(backupSetController.currentId)
                        }
                    }
                }
            }

            Label {
                id: statusLabel
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            Label {
                text: qsTr("Included files (%1)").arg(backupSetController.previewIncluded.length)
                font.bold: true
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
                wrapMode: Text.WordWrap
            }

            Label {
                text: qsTr("Retention cleanup is waiting for confirmation. Proposed deletions:")
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
                    width: parent ? parent.width : 0
                    elide: Text.ElideMiddle
                }
            }

            Button {
                text: qsTr("Confirm proposed cleanup")
                visible: backupSetController.cleanupConfirmationRequired
                onClicked: backupSetController.confirmCleanup()
            }

            RowLayout {
                Layout.fillWidth: true

                TextField {
                    id: discoveryRootField
                    text: backupSetController.currentRemoteRoot
                    placeholderText: qsTr("Remote backup root for discovery")
                    Layout.fillWidth: true
                }

                Button {
                    text: qsTr("Discover remote backups")
                    onClicked: restoreController.discover(discoveryRootField.text)
                }
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

            RowLayout {
                Layout.fillWidth: true

                TextField {
                    id: manifestField
                    placeholderText: qsTr("Path to manifest.json")
                    Layout.fillWidth: true
                }

                Button {
                    text: qsTr("Load restore")
                    onClicked: restoreController.loadManifest(manifestField.text)
                }
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

    Connections {
        target: backupSetController
        function onCurrentSetChanged() { loadCurrentSet() }
        function onStatusChanged(status) { statusLabel.text = status }
        function onFailed(error) { statusLabel.text = error }
    }

    Connections {
        target: backupLauncher
        function onStarted() { statusLabel.text = qsTr("Backup started.") }
        function onFailed(error) { statusLabel.text = error }
    }

    Connections {
        target: restoreController
        function onEntriesChanged() { root.selectedRestoreIndexes = [] }
        function onCopiesChanged() { remoteCopySelector.currentIndex = -1 }
        function onStatusChanged(status) { statusLabel.text = status }
        function onFailed(error) { statusLabel.text = error }
    }
}
