import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "LocalPaths.js" as LocalPaths

GroupBox {
    id: panel
    required property var style
    required property var controller
    property var selectedIndexes: []
    property var selectedPaths: []
    property int selectedCopyIndex: -1
    property string selectedCopyPath: ""
    property var selectionLookup: ({})
    property var contextStates: ({})
    property string contextKey: ""
    property real screenScrollY: -1
    property bool restoringContext: false
    signal completed()
    signal closeRequested()

    objectName: "restorePanel"
    title: qsTr("Restore")
    font.family: style.bodyFontFamily
    font.pixelSize: style.sectionTitleSize
    font.weight: Font.Bold
    padding: style.cardPadding
    Layout.fillWidth: true
    Layout.preferredHeight: Math.max(320, restoreContent.implicitHeight + 32)
    Layout.bottomMargin: style.contentPadding

    function reset() {
        selectedIndexes = []
        selectedPaths = []
        selectedCopyIndex = -1
        selectedCopyPath = ""
        destinationField.text = ""
        restoreList.contentY = 0
        screenScrollY = -1
    }

    function rememberContext() {
        if (contextKey.length === 0) return
        contextStates[contextKey] = {
            paths: selectedPaths.slice(), indexes: selectedIndexes.slice(),
            copyPath: selectedCopyPath, copyIndex: selectedCopyIndex,
            destination: destinationField.text, fileScroll: restoreList.contentY,
            screenScroll: screenScrollY
        }
    }

    function activateContext(folder, setId) {
        const key = JSON.stringify([folder, setId])
        rememberContext()
        if (contextKey === key) return
        restoringContext = true
        contextKey = key
        reset()
        const state = contextStates[key]
        if (state) {
            selectedPaths = state.paths
            selectedIndexes = state.indexes
            selectedCopyPath = state.copyPath
            selectedCopyIndex = state.copyIndex
            destinationField.text = state.destination
            screenScrollY = state.screenScroll
            Qt.callLater(function () {
                if (panel && panel.contextKey === key) restoreList.contentY = state.fileScroll
            })
        }
        restoringContext = false
    }

    onSelectedIndexesChanged: {
        const lookup = Object.create(null)
        selectedIndexes.forEach(function (index) { lookup[index] = true })
        selectionLookup = lookup
    }

    function focusSearch(viewport) {
        function inView(control) {
            if (!control.visible || !control.enabled) return false
            const position = control.mapToItem(viewport, 0, 0)
            return position.y >= 0 && position.y + control.height <= viewport.height
        }
        function firstVisibleControl(item) {
            if (item.activeFocusOnTab && inView(item)) return item
            for (const child of item.children || []) {
                const control = firstVisibleControl(child)
                if (control) return control
            }
            return null
        }
        const target = inView(restoreCopySearch) ? restoreCopySearch : firstVisibleControl(panel)
        if (target) target.forceActiveFocus()
        else viewport.forceActiveFocus()
    }

    ColumnLayout {
        id: restoreContent
        anchors.fill: parent
        spacing: 16

        ActionButton {
            style: panel.style
            objectName: "closeRestoreButton"
            text: qsTr("Back to dashboard")
            Layout.alignment: Qt.AlignRight
            onClicked: panel.closeRequested()
        }

        Label {
            objectName: "restoreInstructions"
            text: qsTr("Choose a backup copy, tick the files you want to restore, and choose the folder to restore them to. Then press Start restore below.")
            font.pixelSize: panel.style.bodyTypeSize
            lineHeight: panel.style.bodyLeading
            lineHeightMode: Text.ProportionalHeight
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        TextField {
            id: restoreCopySearch
            objectName: "restoreCopySearch"
            placeholderText: qsTr("Search computer, backup name, copy, or status")
            text: panel.controller.copySearch
            onTextChanged: panel.controller.copySearch = text
            Layout.fillWidth: true
        }

        ComboBox {
            id: remoteCopySelector
            objectName: "restoreCopySelector"
            model: panel.controller.copies
            currentIndex: panel.controller.currentCopyIndex
            enabled: count > 0
            Layout.fillWidth: true
            // ComboBox defaults to row zero when its model grows. Reapply the
            // controller binding so initial listing keeps an explicit choice,
            // and refreshed/cached selections continue to follow copy identity.
            onModelChanged: currentIndex = Qt.binding(function () { return panel.controller.currentCopyIndex })
            onActivated: {
                if (panel.selectedCopyIndex !== currentIndex) {
                    panel.selectedIndexes = []
                    panel.selectedPaths = []
                }
                panel.selectedCopyIndex = currentIndex
                panel.controller.selectCopy(currentIndex)
            }
            displayText: currentIndex < 0 ? qsTr("Choose a backup copy…") : currentText
        }

        RowLayout {
            visible: panel.controller.busy
            Layout.fillWidth: true

            BusyIndicator {
                objectName: "restoreLoadingIndicator"
                running: panel.controller.busy
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
            }

            Label {
                objectName: "restoreLoadingMessage"
                text: panel.controller.loadingMessage
                color: panel.style.mutedColor
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        Label {
            objectName: "restoreRefreshError"
            text: qsTr("Refresh failed: %1").arg(panel.controller.browseError)
            visible: panel.controller.browseError.length > 0
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            objectName: "restoreCachedDataMessage"
            text: panel.controller.busy
                ? qsTr("Showing the last successful result while refreshing. Cached files cannot be restored until verification finishes.")
                : qsTr("Showing cached data from the last successful refresh. Select the copy to verify it again.")
            visible: panel.controller.showingCachedData
            color: panel.style.mutedColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Unavailable or failed items: %1").arg(panel.controller.unavailableEntries.length)
            font.pixelSize: panel.style.metadataTypeSize
            visible: panel.controller.unavailableEntries.length > 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        ListView {
            model: panel.controller.unavailableEntries
            visible: panel.controller.unavailableEntries.length > 0
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
            font.family: panel.style.bodyFontFamily
            font.pixelSize: panel.style.sectionTitleSize
            font.weight: Font.Bold
            color: panel.style.accentColor
            Layout.fillWidth: true
        }

        ListView {
            id: restoreList
            objectName: "restoreFilesList"
            model: panel.controller.entries
            Layout.fillWidth: true
            Layout.preferredHeight: count > 0 ? Math.max(48, Math.min(180, contentHeight)) : 0
            clip: true
            ScrollBar.vertical: ScrollBar {}
            delegate: CheckBox {
                required property int index
                required property string modelData
                objectName: "restoreFile-" + index
                text: modelData
                width: restoreList.width
                checked: panel.selectionLookup[index] === true
                onToggled: {
                    let selected = panel.selectedIndexes.slice()
                    const position = selected.indexOf(index)
                    if (checked && position < 0) {
                        selected.push(index)
                    } else if (!checked && position >= 0) {
                        selected.splice(position, 1)
                    }
                    panel.selectedIndexes = selected
                    const entries = panel.controller.entries
                    panel.selectedPaths = selected.map(function (selectedIndex) {
                        return entries[selectedIndex]
                    })
                }
            }
        }

        Label {
            text: remoteCopySelector.currentIndex < 0
                ? qsTr("Choose a backup copy to see the files available to restore.")
                : qsTr("This copy has no verified files available to restore.")
            visible: restoreList.count === 0 && !panel.controller.busy
            color: panel.style.mutedColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            objectName: "restoreSelectionCount"
            text: qsTr("Selected files: %1").arg(panel.selectedIndexes.length)
            color: panel.style.mutedColor
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("2. Choose the destination folder")
            font.pixelSize: panel.style.sectionTitleSize
            font.weight: Font.Bold
            color: panel.style.accentColor
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
                style: panel.style
                objectName: "chooseRestoreDestinationButton"
                text: qsTr("Choose folder…")
                onClicked: restoreDestinationDialog.open()
            }
        }

        FolderDialog {
            id: restoreDestinationDialog
            objectName: "restoreDestinationDialog"
            title: qsTr("Choose where to restore the selected files")
            onAccepted: destinationField.text = LocalPaths.localPath(selectedFolder)
        }

        Label {
            text: qsTr("The selected files will be restored into this folder, keeping their backed-up folder structure.")
            font.pixelSize: panel.style.metadataTypeSize
            color: panel.style.mutedColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        ActionButton {
            style: panel.style
            objectName: "startRestoreButton"
            text: qsTr("Start restore")
            Layout.alignment: Qt.AlignRight
            enabled: panel.controller.restoreEligible && panel.selectedIndexes.length > 0 && destinationField.text.trim().length > 0
            onClicked: panel.controller.restoreSelected(panel.selectedIndexes, destinationField.text.trim())
        }
    }

    Connections {
        target: panel.controller
        function onRestoreCompletedForContext(folder, setId, copyPath) {
            if (panel.contextKey !== JSON.stringify([folder, setId])
                || panel.selectedCopyPath !== copyPath) return
            panel.reset()
            delete panel.contextStates[panel.contextKey]
            panel.completed()
        }
        function onEntriesChanged() {
            if (panel.restoringContext) return
            const entries = panel.controller.entries
            const paths = panel.selectedPaths.length > 0
                ? panel.selectedPaths : panel.selectedIndexes.map(function (index) {
                    return entries[index]
                })
            if (entries.length === 0 && panel.controller.browsing) {
                return
            }
            const indexesByPath = Object.create(null)
            entries.forEach(function (path, index) { indexesByPath[path] = index })
            panel.selectedIndexes = paths.map(function (path) {
                return indexesByPath[path] === undefined ? -1 : indexesByPath[path]
            }).filter(function (index) { return index >= 0 })
            panel.selectedPaths = panel.selectedIndexes.map(function (index) {
                return entries[index]
            })
        }
        function onCurrentCopyIndexChanged() {
            if (panel.restoringContext) return
            const path = panel.controller.currentCopyPath
            if (panel.selectedCopyPath !== path && !(path.length === 0 && panel.controller.browsing)) {
                panel.selectedIndexes = []
                panel.selectedPaths = []
                panel.selectedCopyPath = path
            }
            panel.selectedCopyIndex = panel.controller.currentCopyIndex
        }
    }
}
