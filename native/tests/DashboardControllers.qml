import QtQuick

QtObject {
    property QtObject backupSetController: QtObject {
        property var setNames: ["Documents", "Photos"]
        property var setIds: ["documents-id", "photos-id"]
        property int currentIndex: 0
        property string currentId: setIds[currentIndex] || ""
        property string currentName: setNames[currentIndex] || ""
        property string currentRemoteRoot: "/backups/" + currentId
        property var currentSources: ["/safe/" + currentId]
        property var currentExclusions: []
        property string currentScheduleFrequency: "disabled"
        property int currentScheduleHour: 2
        property int currentScheduleMinute: 0
        property int currentScheduleWeekday: 1
        property int currentScheduleDayOfMonth: 1
        property int currentRetention: 3
        property bool currentOnlyOnAcPower: false
        property var currentRequiredMounts: []
        property string currentNextRun: "Disabled"
        property string currentRunStatus: "idle"
        property string currentRunError: ""
        property var runningSetIds: []
        property var recentBackups: ["Photos\nNo backup run yet", "Documents\nsucceeded"]
        property var recentBackupSetIds: ["photos-id", "documents-id"]
        property var recentBackupTimestamps: ["", "2026-10-01 10:00"]
        property var previewIncluded: []
        property var previewExcluded: []
        property var previewSkipped: []
        property var previewMissing: []
        property var cleanupTargets: []
        property bool cleanupConfirmationRequired: false
        property int removedIndex: -1
        property int addedCount: 0
        signal currentSetChanged()
        signal statusChanged(string status)
        signal failed(string error)
        onCurrentIndexChanged: currentSetChanged()
        function removeSet(index) { removedIndex = index }
        function removeCurrentSet() { removeSet(currentIndex) }
        function addSet() {
            addedCount++
            setIds = setIds.concat(["new-id"])
            setNames = setNames.concat(["New set"])
            currentIndex = setNames.length - 1
        }
        function preview() {}
        function save() { return true }
        function confirmCleanup() { return true }
    }

    property QtObject backupLauncher: QtObject {
        property string launchedId: ""
        signal started()
        signal failed(string error)
        function startBackup(id) { launchedId = id }
    }

    property QtObject restoreController: QtObject {
        property var entries: []
        property var copies: []
        property var unavailableEntries: []
        property string copySearch: ""
        property string defaultDestination: "/safe/restore"
        property string discoveredRoot: ""
        signal statusChanged(string status)
        signal failed(string error)
        function discover(remoteRoot) { discoveredRoot = remoteRoot }
        function selectCopy(index) {}
        function restoreSelected(indexes, destination) {}
        function restoreFolder(folder, destination) {}
    }
}
