import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: dashboard
    required property var style
    required property var controller
    required property var launcher
    required property var copies
    required property var folderBrowser
    required property var restoreState
    required property real windowHeight
    signal newSetRequested()
    signal editSetRequested(int index)
    signal removeSetRequested(int index)
    signal restoreRequested(int index)
    signal openFolderRequested(int index)
    signal detailsRequested(string setId)

    Layout.fillWidth: true
    Layout.preferredHeight: 56 + Math.max(180, Math.min(360,
        windowHeight - 280,
        Math.max(dashboardSetsList.contentHeight, recentBackupsList.contentHeight)))
    spacing: 32

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: (dashboard.width - dashboard.spacing) / 3
        Layout.minimumWidth: 0
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40

            Label {
                objectName: "backupSetsTitle"
                text: qsTr("Backup sets")
                font.pixelSize: dashboard.style.sectionTitleSize
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }

            ActionButton {
                style: dashboard.style
                objectName: "newBackupSetButton"
                text: "+"
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                Accessible.name: qsTr("New backup set")
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: dashboard.newSetRequested()
            }
        }

        ListView {
            id: dashboardSetsList
            objectName: "dashboardSetsList"
            model: dashboard.controller.setNames
            clip: true
            spacing: 10
            Layout.fillWidth: true
            Layout.fillHeight: true
            ScrollBar.vertical: ScrollBar {}

            Label {
                objectName: "emptySetsLabel"
                text: qsTr("No backups yet")
                color: dashboard.style.mutedColor
                visible: dashboardSetsList.count === 0
                width: parent.width
                padding: 16
            }

            delegate: Frame {
                id: setRow
                required property int index
                required property string modelData
                readonly property bool runActive: dashboard.controller.runningSetIds.indexOf(dashboard.controller.setIds[index]) >= 0
                width: dashboardSetsList.width
                implicitHeight: Math.max(80, setSummary.implicitHeight + 32)
                padding: 16
                background: Rectangle {
                    color: dashboard.style.backgroundColor
                    radius: 8
                    border.color: dashboard.style.lineColor
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
                            text: setRow.modelData
                            font.pixelSize: dashboard.style.bodyTypeSize
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                        }

                        Label {
                            objectName: "setRemainingTime-" + setRow.index
                            text: dashboard.controller.remainingTimes[dashboard.controller.setIds[setRow.index]]
                                || qsTr("Estimating time remaining…")
                            visible: setRow.runActive
                            font.pixelSize: dashboard.style.metadataTypeSize
                            color: dashboard.style.mutedColor
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                        }

                        Label {
                            objectName: "setTransferProgress-" + setRow.index
                            text: (dashboard.controller.transferProgress[dashboard.controller.setIds[setRow.index]] || {}).text || ""
                            visible: setRow.runActive && text.length > 0
                            textFormat: Text.PlainText
                            font.pixelSize: dashboard.style.metadataTypeSize
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                        }

                        ProgressBar {
                            objectName: "setProgressBar-" + setRow.index
                            readonly property var progress: dashboard.controller.transferProgress[dashboard.controller.setIds[setRow.index]] || ({})
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
                        style: dashboard.style
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
                                onTriggered: dashboard.editSetRequested(setRow.index)
                            }

                            MenuItem {
                                text: qsTr("Back up now")
                                enabled: setRow.index < dashboard.controller.setIds.length && !setRow.runActive
                                onTriggered: dashboard.launcher.startBackup(dashboard.controller.setIds[setRow.index])
                            }

                            MenuSeparator {}

                            MenuItem {
                                text: qsTr("Delete")
                                onTriggered: dashboard.removeSetRequested(setRow.index)
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
        Layout.preferredWidth: 2 * (dashboard.width - dashboard.spacing) / 3
        Layout.minimumWidth: 0
        spacing: 16

        Label {
            objectName: "recentBackupsTitle"
            text: qsTr("Recent backups")
            font.pixelSize: dashboard.style.sectionTitleSize
            font.weight: Font.DemiBold
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            verticalAlignment: Text.AlignVCenter
        }

        ListView {
            id: recentBackupsList
            objectName: "recentBackupsList"
            model: dashboard.controller.recentBackups
            implicitWidth: 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 12
            ScrollBar.vertical: ScrollBar {}

            Label {
                objectName: "emptyRecentLabel"
                text: qsTr("No backups yet")
                color: dashboard.style.mutedColor
                visible: recentBackupsList.count === 0
                width: parent.width
                padding: 16
            }

            delegate: Frame {
                id: recentRow
                required property int index
                required property string modelData
                readonly property string timestamp: dashboard.controller.recentBackupTimestamps[index] || ""
                readonly property var details: dashboard.controller.runDetails[dashboard.controller.recentBackupSetIds[index]] || ({})
                width: recentBackupsList.width
                implicitHeight: Math.max(80, recentText.implicitHeight + 40)
                padding: 20
                background: Rectangle {
                    color: dashboard.style.softColor
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
                            font.pixelSize: dashboard.style.bodyTypeSize
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
                            font.pixelSize: dashboard.style.metadataTypeSize
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }

                    ActionButton {
                        style: dashboard.style
                        objectName: "openFolder-" + recentRow.index
                        text: "↗"
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        Accessible.name: qsTr("Open %1 in Proton Drive")
                            .arg(dashboard.controller.recentBackups[recentRow.index].split("\n")[0])
                        ToolTip.visible: hovered
                        ToolTip.text: Accessible.name
                        enabled: recentRow.timestamp.length > 0 && !dashboard.folderBrowser.busy && !dashboard.copies.busy
                            && dashboard.controller.setIds.indexOf(dashboard.controller.recentBackupSetIds[recentRow.index]) >= 0
                        onClicked: dashboard.openFolderRequested(recentRow.index)
                    }

                    ActionButton {
                        style: dashboard.style
                        objectName: "restore-" + recentRow.index
                        text: qsTr("Restore")
                        Layout.preferredHeight: 36
                        enabled: recentRow.timestamp.length > 0 && !dashboard.restoreState.busy
                            && dashboard.controller.setIds.indexOf(dashboard.controller.recentBackupSetIds[recentRow.index]) >= 0
                        onClicked: dashboard.restoreRequested(recentRow.index)
                    }

                    ActionButton {
                        id: recentActionsButton
                        style: dashboard.style
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
                                enabled: recentRow.timestamp.length > 0 && !dashboard.copies.busy
                                    && dashboard.controller.setIds.indexOf(dashboard.controller.recentBackupSetIds[recentRow.index]) >= 0
                                    && dashboard.controller.runningSetIds.indexOf(dashboard.controller.recentBackupSetIds[recentRow.index]) < 0
                                onTriggered: dashboard.copies.requestDelete(dashboard.controller.recentBackupSetIds[recentRow.index])
                            }

                            MenuItem {
                                objectName: "viewBackupDetails-" + recentRow.index
                                text: qsTr("View details")
                                enabled: (recentRow.details.status || "").length > 0
                                onTriggered: dashboard.detailsRequested(dashboard.controller.recentBackupSetIds[recentRow.index])
                            }
                        }
                    }
                }
            }
        }
    }
}
