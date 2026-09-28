import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    visible: true
    width: 760
    height: 520
    title: qsTr("Praefectus")

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 640)
        spacing: 16

        Label {
            text: qsTr("First backup")
            font.pixelSize: 28
        }

        Label {
            text: qsTr("Select a folder to preview files before connecting a Proton Drive destination.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: sourceField
                placeholderText: qsTr("Absolute source folder")
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Preview")
                onClicked: {
                    const files = backupEngine.selectableFiles(sourceField.text)
                    fileList.model = files
                    statusLabel.text = files.length === 0
                        ? qsTr("No regular files found.")
                        : qsTr("%1 files selected.").arg(files.length)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: remoteField
                placeholderText: qsTr("Remote backup folder")
                text: "/my-files/backups/first-copy"
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Back up")
                enabled: fileList.count > 0
                onClicked: {
                    statusLabel.text = qsTr("Backup is not connected to the Proton worker yet.")
                }
            }
        }

        Label {
            id: statusLabel
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        ListView {
            id: fileList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: []
            delegate: Label {
                required property string modelData
                text: modelData
                elide: Text.ElideMiddle
                width: fileList.width
            }
        }
    }
}
