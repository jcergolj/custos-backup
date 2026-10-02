import QtQuick

QtObject {
    property QtObject backupScheduler: QtObject {
        property bool busy: false
        property bool ready: true
        property bool hasSchedules: true
        property string status: "Scheduling active"
        property string error: ""
        property int enableCount: 0
        property int refreshCount: 0
        signal messageChanged(string message)
        signal failed(string error)
        function enable() { enableCount++ }
        function refresh() { refreshCount++ }
    }

    property QtObject resourceUsage: QtObject {
        property var names: ["Very low", "Low", "Medium", "High", "Very high"]
        property var descriptions: ["CPU limit: 10%", "CPU limit: 25%", "CPU limit: 50%", "CPU limit: 100%", "CPU limit: 200%"]
        property int presetIndex: -1
        property bool busy: false
        property int savedIndex: -2
        signal presetChanged()
        signal statusChanged(string message)
        signal failed(string error)
        function save(index) { savedIndex = index }
    }

    property QtObject protonAuth: QtObject {
        property bool authenticated: true
        property bool checked: true
        property bool checking: false
        property bool cliAvailable: true
        property string error: ""
        property int signInCount: 0
        property int refreshCount: 0
        signal statusChanged(string message)
        signal failed(string error)
        function signIn() { signInCount++ }
        function refresh() { refreshCount++ }
    }

    property QtObject themeColors: QtObject {
        property var colors: ({
            background: "#ffffff", foreground: "#19232e", muted: "#586575",
            surface: "#f0f3f6", border: "#dce1e7", accent: "#245bcb",
            highlight: "#245bcb", highlightedText: "#ffffff", brightText: "#ffffff"
        })
    }

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
        property var remainingTimes: ({})
        property var recentBackups: ["Photos\nNo backup run yet", "Documents\nsucceeded"]
        property var recentBackupSetIds: ["photos-id", "documents-id"]
        property var recentBackupTimestamps: ["", "01/10/2026 10:00:00"]
        property var previewIncluded: []
        property var previewExcluded: []
        property var previewSkipped: []
        property var previewMissing: []
        property var cleanupTargets: []
        property bool cleanupConfirmationRequired: false
        property int removedIndex: -1
        property int addedCount: 0
        property int refreshCount: 0
        property string importedPath: ""
        property string exportedPath: ""
        property bool importSucceeds: true
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
        function recentBackupFolderPath(setId) {
            return setIds.indexOf(setId) >= 0 ? "/backups/" + setId : ""
        }
        function save() { return true }
        function importSets(path) { importedPath = path; return importSucceeds }
        function exportSets(path) { exportedPath = path; return true }
        function confirmCleanup() { return true }
        function refreshRunState() { refreshCount++ }
    }

    property QtObject backupLauncher: QtObject {
        property string launchedId: ""
        signal started()
        signal failed(string error)
        function startBackup(id) { launchedId = id }
    }

    property QtObject protonFolderBrowser: QtObject {
        property bool busy: false
        property string requestedPath: ""
        signal folderResolved(url folderUrl)
        signal failed(string error)
        function openFolder(path) { requestedPath = path }
    }

    property QtObject recentBackupCopies: QtObject {
        property bool busy: false
        property string openedId: ""
        property string deleteRequestedId: ""
        property bool deleteConfirmed: false
        property bool deleteCancelled: false
        signal folderResolved(string path)
        signal deleteConfirmationReady(string name, string path)
        signal copyDeleted(string setId)
        signal statusChanged(string message)
        signal failed(string error)
        function openCopy(setId) { openedId = setId }
        function requestDelete(setId) {
            deleteRequestedId = setId
            deleteConfirmationReady("Documents", "/backups/documents-id/copy-id")
        }
        function confirmDelete() { deleteConfirmed = true }
        function cancelDelete() { deleteCancelled = true }
    }

    property QtObject restoreController: QtObject {
        property bool busy: false
        property string loadingMessage: ""
        property bool showingCachedData: false
        property bool restoreEligible: true
        property int currentCopyIndex: -1
        property int selectedCopy: -1
        property var entries: []
        property var copies: []
        property var unavailableEntries: []
        property string copySearch: ""
        property string defaultDestination: "/safe/restore"
        property string discoveredRoot: ""
        property string discoveredSetId: ""
        property var restoredIndexes: []
        property string restoreDestination: ""
        property int restoreCount: 0
        property bool restoreSucceeds: true
        signal statusChanged(string status)
        signal failed(string error)
        signal restoreCompleted()
        function discover(remoteRoot, setId) { discoveredRoot = remoteRoot; discoveredSetId = setId }
        function selectCopy(index) { selectedCopy = index; currentCopyIndex = index }
        function restoreSelected(indexes, destination) {
            restoredIndexes = indexes.slice()
            restoreDestination = destination
            restoreCount++
            if (restoreSucceeds) {
                restoreCompleted()
            } else {
                failed("Restore failed")
            }
        }
        function restoreFolder(folder, destination) {}
    }
}
