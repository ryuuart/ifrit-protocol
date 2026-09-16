// Delegates reach outward — a row needs the window's rows and its
// selection. Bound makes those captures explicit and well-defined rather
// than resolved by scope-chain accident.
pragma ComponentBehavior: Bound

// THE BROWSER STANDS BESIDE THE CANVAS, NEVER IN FRONT OF IT.
//
// Going through a registry this size is a matter of looking at one
// sketch after another, and a browser that took the window would make
// every look a round trip through a screen with no pictures on it. So
// the canvas keeps presenting while the browser is read: SELECTION is a
// look — it moves the inspector and nothing else — and ENTER is what
// moves the canvas. The resident set is what makes that worth doing,
// because a sketch already opened comes back without being compiled again;
// its runtime session is fresh so its entrance animation still plays.

import QtCore
import QtQuick
import Ifrit.Qt 1.0 as Ui
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Sigil.Sketchbook

ApplicationWindow {
    id: window

    width: 1440
    height: 920
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: actions.workspaceName.length > 0 ? actions.workspaceName + " — Sketchbook" : "Sketchbook"
    color: Ui.Theme.windowBackground

    font.pixelSize: Ui.Theme.bodySize
    background: null

    // ---- What the window remembers between runs --------------------------
    Settings {
        id: settings

        category: "browser"
        property string viewMode: "gallery"
        property real browserWidth: 380
        property string sortKey: "name"
        property bool sortAscending: true
        property string groupMode: "subjects"
        property string groupPath: ""
        property var expandedGroups: ({})
    }

    // ---- The rows, and what the window remembers about them --------------
    Browser {
        id: browser

        catalog: catalog
        listView: sketchList
        galleryView: gallery
    }

    property bool restoringSettings: true
    /** The sketch the canvas opens on — named on the command line, or the
     *  first row. */
    readonly property int openAt: catalog.openIndex

    // What the reader last set is written back as it changes, so the
    // window comes up on the browser they left.
    Connections {
        target: browser
        function onGroupModeChanged() {
            if (!window.restoringSettings)
                settings.groupMode = browser.groupMode;
        }
        function onGroupPathChanged() {
            if (!window.restoringSettings)
                settings.groupPath = browser.groupPath;
        }
        function onExpandedGroupsChanged() {
            if (!window.restoringSettings)
                settings.expandedGroups = browser.expandedGroups;
        }
        function onViewModeChanged() {
            settings.viewMode = browser.viewMode;
        }
        function onSortKeyChanged() {
            settings.sortKey = browser.sortKey;
        }
        function onSortAscendingChanged() {
            settings.sortAscending = browser.sortAscending;
        }
    }

    /** Present it. The one action that moves the canvas — and the one
     *  that ends the thumbnail fill, because from here on the canvas is
     *  what draws and a second renderer beside it is what would make
     *  this feel slow. */
    function activate(index) {
        if (index < 0)
            return;
        catalog.endFill();
        if (view.sketchIndex === index)
            view.replay();
        else
            view.sketchIndex = index;
    }

    FileDialog {
        id: sketchDialog

        title: "Open Sketch"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Sketch files (*.cpp *.py)", "C++ sketches (*.cpp)", "Python sketches (*.py)"]
        currentFolder: actions.openFolder
        onAccepted: actions.openFile(selectedFile)
    }

    FolderDialog {
        id: workspaceDialog

        title: "Open Workspace"
        currentFolder: actions.openFolder
        onAccepted: actions.openWorkspace(selectedFolder)
    }

    // ---- Captures --------------------------------------------------------
    property string captureLine: ""
    Timer {
        id: captureHide

        interval: 2500
        onTriggered: window.captureLine = ""
    }
    function showCapture(path) {
        window.captureLine = path.length > 0 ? "saved " + path.split("/").pop() : "capture failed";
        captureHide.restart();
    }

    FileDialog {
        id: videoDialog

        property var sketch: ({})
        fileMode: FileDialog.SaveFile
        nameFilters: ["MPEG-4 video (*.mp4)"]
        defaultSuffix: "mp4"
        acceptLabel: "Export"
        onAccepted: actions.video(videoDialog.sketch, selectedFile)
    }

    function exportVideo(index) {
        videoDialog.sketch = index < 0 ? ({}) : browser.sketchAt(index);
        videoDialog.currentFile = actions.videoDefault(videoDialog.sketch);
        videoDialog.open();
    }

    Shortcut {
        sequences: [StandardKey.Open]
        enabled: !actions.opening
        onActivated: sketchDialog.open()
    }
    Shortcut {
        // The plural form: Save is more than one binding on some
        // platforms, and binding the first silently drops the rest.
        sequences: [StandardKey.Save]
        onActivated: view.capture()
    }
    Action {
        id: publicationAction
        text: "Publish"
        shortcut: "Ctrl+P"
        checkable: true
        checked: view.publishing
        onTriggered: view.publishing = checked
    }
    Shortcut {
        sequence: "/"
        enabled: !librarySearch.activeFocus && !view.canvasFocused
        onActivated: topBar.focusFilter()
    }
    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: topBar.focusFilter()
    }
    Shortcut {
        sequence: "Ctrl+I"
        onActivated: detailsDrawer.visible ? detailsDrawer.close() : detailsDrawer.open()
    }

    // ---- Everything a sketch says about itself ----------------------------
    SketchCatalog {
        id: catalog
    }
    SketchActions {
        id: actions
    }
    property bool openNoticeDismissed: false
    property bool taskNoticeDismissed: false
    Connections {
        target: actions
        function onOpenChanged() {
            window.openNoticeDismissed = false;
        }
        function onTaskChanged() {
            window.taskNoticeDismissed = false;
        }
    }

    // A running session is the only thing that knows the canvas a sketch
    // declared, the ground behind it and the moment it names — those are
    // stated from inside its own setup. Every frame publishes them, and
    // the catalog keeps the answer, so a row reads its canvas back long
    // after the resident set has let the session go.
    Connections {
        target: view
        function onMetricsChanged() {
            const stats = view.metrics;
            if (stats.sketchIndex === undefined)
                return;
            const learned = catalog.learn(stats.sketchIndex, stats.canvas ?? "", stats.moment ?? -1, stats.background ?? "", stats.runtime ?? "");
            if (learned.sketchIndex === undefined)
                return;
            browser.overlayRow(learned);
        }
        function onSketchIndexChanged() {
            if (browser.rowForSketch(view.sketchIndex) >= 0)
                browser.selectedIndex = view.sketchIndex;
            const row = browser.sketchAt(view.sketchIndex);
            if (row !== undefined)
                actions.noteSelection(row);
        }
    }

    // A thumbnail landed, or a sketch has none and there is a line
    // saying why. The row is overlaid by index so exactly one card
    // changes — the reason learn() and the fill both route through here
    // rather than resetting the whole model, which would remount every
    // other thumbnail.
    Connections {
        target: catalog
        function onThumbnailReady(index, row) {
            browser.overlayRow(row);
        }
        function onThumbnailNoted(name, why) {
            window.captureLine = name + " — " + why;
            captureHide.restart();
        }
        // THE FILL IS OVER AND NOTHING IS PRESENTED YET: the canvas opens
        // on what this run was pointed at. A sketch opened while the fill
        // was running got here first and ended it, and the canvas is
        // already showing that one.
        function onFillChanged() {
            if (!catalog.filling && view.sketchIndex < 0)
                window.activate(window.openAt);
        }
    }

    // The sketch on screen reached the moment it declared and left its
    // still in the store: the row reads it back, so browsing is what
    // keeps the thumbnails current.
    Connections {
        target: view
        function onThumbnailCaptured(index) {
            catalog.adoptThumbnail(index);
        }
    }

    Component.onCompleted: {
        browser.viewMode = settings.viewMode;
        browser.sortKey = settings.sortKey;
        browser.sortAscending = settings.sortAscending;
        browser.expandedGroups = settings.expandedGroups;
        browser.groupMode = settings.groupMode;
        browser.groupPath = settings.groupPath;
        browser.openOn(window.openAt);
        window.restoringSettings = false;
        // THE LOADING PHASE. Every sketch with no still gets one while
        // nothing is being presented, which is the only stretch in which
        // the machine is the fill's alone; opening a sketch ends it, and
        // the canvas opens by itself when it finishes.
        if (!catalog.openAtOnce)
            catalog.fillThumbnails();
        if (!catalog.filling)
            window.activate(window.openAt);
    }

    // Browsing and drawing have independent space; details are summoned over
    // the canvas only when the reader asks for them.
    Drawer {
        id: detailsDrawer
        objectName: "sketchDetails"
        edge: Qt.RightEdge
        width: Math.min(380, window.width - 32)
        height: window.height
        padding: Ui.Theme.sectionSpacing
        modal: false
        focus: true
        dim: false
        background: Rectangle {
            color: Ui.Theme.windowBackground
        }
        contentItem: ColumnLayout {
            spacing: Ui.Theme.spacing
            RowLayout {
                Layout.fillWidth: true
                Ui.SectionHeading {
                    Layout.fillWidth: true
                    text: "SKETCH DETAILS"
                }
                Ui.IconButton {
                    text: "×"
                    tooltip: "Close sketch details"
                    onClicked: detailsDrawer.close()
                }
            }
            Inspector {
                id: inspector
                Layout.fillWidth: true
                Layout.fillHeight: true
                catalog: catalog
                sketch: browser.selectedSketch
                presented: browser.selectedIndex === view.sketchIndex
                metrics: view.metrics
                taskLine: actions.taskLine
                taskRunning: actions.taskRunning
                onOpenRequested: window.activate(browser.selectedIndex)
                onFrameRequested: actions.frame(browser.selectedSketch)
                onVideoRequested: window.exportVideo(browser.selectedIndex)
                onBenchRequested: actions.bench(browser.selectedSketch)
                onRevealRequested: actions.reveal(browser.selectedSketch)
                onTagRequested: path => {
                    browser.chooseMode("subjects");
                    browser.chooseGroup(path);
                    detailsDrawer.close();
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Ui.Theme.sectionSpacing
        spacing: Ui.Theme.sectionSpacing

        TopBar {
            id: topBar
            Layout.fillWidth: true
            workspaceName: actions.workspaceName
            inspectorOpen: detailsDrawer.visible
            taskRunning: actions.taskRunning
            opening: actions.opening
            recents: actions.recents
            onInspectorToggled: detailsDrawer.visible ? detailsDrawer.close() : detailsDrawer.open()
            onVideoRequested: window.exportVideo(-1)
            onOpenFileRequested: sketchDialog.open()
            onOpenWorkspaceRequested: workspaceDialog.open()
            onRecentRequested: recent => actions.openRecent(recent)
            onClearRecentsRequested: actions.clearRecents()
            onRecentsRequested: actions.refreshRecents()
            onSearchRequested: {
                librarySearch.forceActiveFocus();
                librarySearch.selectAll();
            }
        }

        Ui.Notice {
            Layout.fillWidth: true
            visible: !window.openNoticeDismissed && (actions.openStatus.length > 0 || actions.openError.length > 0)
            text: actions.openError.length > 0 ? actions.openError : actions.openStatus
            tone: actions.openError.length > 0 ? "error" : "neutral"
            busy: actions.opening
            dismissible: !actions.opening
            onDismissed: window.openNoticeDismissed = true
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal
            handle: Item {
                implicitWidth: Ui.Theme.sectionSpacing
                Rectangle {
                    anchors.centerIn: parent
                    width: 3
                    height: 32
                    radius: 1.5
                    color: SplitHandle.pressed ? Ui.Theme.accent : SplitHandle.hovered ? Ui.Theme.secondaryText : Ui.Theme.border
                }
            }

            Ui.Panel {
                id: browserPane
                objectName: "libraryPanel"
                SplitView.preferredWidth: Math.min(settings.browserWidth, Math.max(320, window.width * 0.32), 500)
                SplitView.minimumWidth: 320
                SplitView.maximumWidth: 560
                padding: 0
                backgroundColor: Ui.Theme.panelBackground
                Component.onDestruction: settings.browserWidth = browserPane.width

                contentItem: ColumnLayout {
                    spacing: 0
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.margins: Ui.Theme.sectionSpacing
                        spacing: Ui.Theme.spacing
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: "Library"
                                color: Ui.Theme.primaryText
                                font.pixelSize: Ui.Theme.headingSize
                                font.weight: Font.DemiBold
                                Layout.fillWidth: true
                            }
                            Label {
                                text: browser.cards.length + (browser.cards.length === 1 ? " sketch" : " sketches")
                                color: Ui.Theme.secondaryText
                                font.pixelSize: Ui.Theme.captionSize
                            }
                        }
                        Ui.SearchField {
                            id: librarySearch
                            objectName: "librarySearch"
                            Layout.fillWidth: true
                            text: browser.filterText
                            placeholderText: "Search sketches or tags"
                            onTextEdited: browser.filterText = text
                            onSteppedOut: browser.viewMode === "gallery" ? gallery.focusRows() : sketchList.focusRows()
                            onAccepted: browser.viewMode === "gallery" ? gallery.focusRows() : sketchList.focusRows()
                            ToolTip.visible: hovered && !activeFocus
                            ToolTip.delay: 900
                            ToolTip.text: "Search names and descriptions, or use tag:, kind:, folder:"
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Ui.Theme.spacing
                            Ui.IconButton {
                                id: groupButton
                                objectName: "groupPicker"
                                Layout.fillWidth: true
                                Layout.minimumWidth: 60
                                text: (browser.groupPath.split("/").pop() || "All sketches") + " ▾"
                                tooltip: "Browse subjects and collections"
                                onClicked: groupPopup.open()
                                Popup {
                                    id: groupPopup
                                    objectName: "groupPopup"
                                    y: groupButton.height + Ui.Theme.spacing
                                    width: 288
                                    height: Math.min(560, window.height - 220)
                                    padding: Ui.Theme.spacing
                                    modal: false
                                    focus: true
                                    background: Rectangle {
                                        radius: Ui.Theme.panelRadius
                                        color: Ui.Theme.solidPanelBackground
                                        border.color: Ui.Theme.border
                                    }
                                    contentItem: GroupTree {
                                        rows: browser.navigationRows
                                        mode: browser.groupMode
                                        selectedPath: browser.groupPath
                                        count: browser.matchingCount
                                        onGroupRequested: path => {
                                            browser.chooseGroup(path);
                                            groupPopup.close();
                                        }
                                        onModeRequested: mode => browser.chooseMode(mode)
                                        onBranchToggled: path => browser.toggleGroup(path)
                                    }
                                }
                            }
                            Ui.SegmentedControl {
                                model: [
                                    {
                                        text: "Gallery"
                                    },
                                    {
                                        text: "List"
                                    }
                                ]
                                currentIndex: browser.viewMode === "gallery" ? 0 : 1
                                Accessible.name: "Library view"
                                onActivated: index => browser.chooseView(index === 0 ? "gallery" : "list")
                            }
                            Ui.IconButton {
                                id: sortButton
                                text: "↕"
                                tooltip: "Sort sketches"
                                onClicked: sortMenu.popup()
                                Menu {
                                    id: sortMenu
                                    y: sortButton.height
                                    Repeater {
                                        model: browser.sortOptions
                                        delegate: MenuItem {
                                            required property var modelData
                                            text: modelData.label
                                            checkable: true
                                            checked: browser.sortKey === modelData.key
                                            onTriggered: browser.sortKey = modelData.key
                                        }
                                    }
                                    MenuSeparator {}
                                    MenuItem {
                                        text: "Ascending"
                                        checkable: true
                                        checked: browser.sortAscending
                                        onTriggered: browser.sortAscending = !browser.sortAscending
                                    }
                                }
                            }
                        }
                        RowLayout {
                            visible: browser.hasFilters
                            Layout.fillWidth: true
                            Label {
                                Layout.fillWidth: true
                                text: browser.groupPath.length ? browser.groupPath.split("/").join(" › ") : "Search results"
                                color: Ui.Theme.secondaryText
                                font.pixelSize: Ui.Theme.captionSize
                                elide: Text.ElideRight
                            }
                            Ui.IconButton {
                                text: "Clear"
                                tooltip: "Clear search and group filters"
                                onClicked: browser.clearFilters()
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: Ui.Theme.separator
                    }
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        SketchList {
                            id: sketchList
                            anchors.fill: parent
                            visible: browser.viewMode === "list"
                            catalog: catalog
                            rows: browser.cards
                            learnedSketches: browser.learnedSketches
                            selectedIndex: browser.selectedIndex
                            presentedIndex: view.sketchIndex
                            sortKey: browser.sortKey
                            sortAscending: browser.sortAscending
                            onSelectRequested: index => browser.select(index)
                            onActivateRequested: index => window.activate(index)
                            onSortRequested: key => browser.sortBy(key)
                            onStepRequested: delta => browser.step(delta)
                        }
                        SketchGallery {
                            id: gallery
                            anchors.fill: parent
                            visible: browser.viewMode === "gallery"
                            catalog: catalog
                            cards: browser.cards
                            learnedSketches: browser.learnedSketches
                            selectedIndex: browser.selectedIndex
                            presentedIndex: view.sketchIndex
                            onSelectRequested: index => browser.select(index)
                            onActivateRequested: index => window.activate(index)
                            onStepRequested: delta => browser.step(delta)
                        }
                        ColumnLayout {
                            anchors.centerIn: parent
                            width: parent.width - 48
                            visible: browser.cards.length === 0
                            spacing: Ui.Theme.spacing
                            Label {
                                Layout.fillWidth: true
                                text: "No matching sketches"
                                color: Ui.Theme.primaryText
                                font.pixelSize: Ui.Theme.headingSize
                                horizontalAlignment: Text.AlignHCenter
                            }
                            Label {
                                Layout.fillWidth: true
                                text: "Try another search or clear the filters."
                                color: Ui.Theme.secondaryText
                                font.pixelSize: Ui.Theme.bodySize
                                background: null
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                            }
                            Button {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Clear filters"
                                visible: browser.hasFilters
                                onClicked: browser.clearFilters()
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: Ui.Theme.separator
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: Ui.Theme.sectionSpacing
                        visible: browser.cards.length > 0
                        spacing: Ui.Theme.spacing
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: Ui.Theme.smallSpacing
                            Label {
                                Layout.fillWidth: true
                                text: (browser.selectedSketch.name ?? "Select a sketch").replace(/_/g, " ")
                                color: Ui.Theme.primaryText
                                font.pixelSize: Ui.Theme.bodySize
                                background: null
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                text: browser.selectedIndex === view.sketchIndex ? "On canvas" : "Enter to open"
                                color: Ui.Theme.secondaryText
                                font.pixelSize: Ui.Theme.captionSize
                            }
                        }
                        Ui.IconButton {
                            text: "Details"
                            tooltip: "Inspect selected sketch"
                            onClicked: detailsDrawer.open()
                        }
                        Button {
                            text: browser.selectedIndex === view.sketchIndex ? "Replay" : "Open"
                            enabled: browser.selectedIndex >= 0 && !!browser.selectedSketch.available
                            implicitHeight: Ui.Theme.controlHeight
                            Accessible.name: text + " selected sketch"
                            onClicked: window.activate(browser.selectedIndex)
                        }
                    }
                }
            }

            CanvasPane {
                id: view
                SplitView.fillWidth: true
                SplitView.minimumWidth: 380
                onCaptureReady: path => window.showCapture(path)
                onThumbnailCaptured: index => catalog.adoptThumbnail(index)
            }
        }

        Ui.Notice {
            objectName: "taskNotice"
            Layout.fillWidth: true
            visible: actions.taskLine.length > 0 && !window.taskNoticeDismissed
            text: actions.taskLine
            busy: actions.taskRunning
            dismissible: !actions.taskRunning
            onDismissed: window.taskNoticeDismissed = true
        }

        StatusStrip {
            Layout.fillWidth: true
            hostState: view.hostState
            status: view.status
            filling: catalog.filling
            fillDone: catalog.fillDone
            fillTotal: catalog.fillTotal
            fillNote: catalog.fillNote
            sketch: view.metrics.sketch ?? ""
            path: browser.sketchAt(view.sketchIndex)?.path ?? ""
            hints: "↑↓ select · ⏎ open · / filter" + (view.orbitable ? " · drag/wheel orbit · ctrl-wheel zoom" : " · wheel zoom · middle-drag pan") + (view.canvasFocused ? " · keys go to the sketch" : "")
            paused: view.paused
            timeScale: view.timeScale
            metrics: view.metrics
            capture: window.captureLine
            publication: view.publishing ? (view.metrics.publish ?? "") : ""
            publicationError: view.publicationError
            publishAction: publicationAction
            onPauseToggled: view.paused = !view.paused
            onCaptureRequested: view.capture()
            onTimeScaleRequested: scale => view.timeScale = scale
        }
    }
}
