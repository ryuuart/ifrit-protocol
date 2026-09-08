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
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import Ifrit.Ui 1.0 as Ui
import Sigil.Sketchbook

ApplicationWindow {
    id: window

    width: 1440
    height: 920
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "Sketchbook"
    color: Theme.ground

    // The Basic style paints its controls straight from the palette;
    // without this a light-grey Button and Slider sit in the middle of a
    // dark panel.
    palette.window: Theme.panel
    palette.windowText: Theme.text
    palette.button: "#241f3d"
    palette.buttonText: Theme.text
    palette.mid: "#2f2951"
    palette.midlight: Theme.border
    palette.dark: Theme.ground
    palette.light: Theme.border
    palette.highlight: Theme.accent
    palette.text: Theme.text
    palette.base: Theme.ground

    // ---- What the window remembers between runs --------------------------
    Settings {
        id: settings

        category: "browser"
        property string viewMode: "list"
        property bool inspectorOpen: true
        property real browserWidth: 540
        property string sortKey: "folder"
        property bool sortAscending: true
    }

    // ---- The rows, and what the window remembers about them --------------
    Browser {
        id: browser

        catalog: catalog
        listView: sketchList
        galleryView: gallery
    }

    property bool inspectorOpen: true
    /** The sketch the canvas opens on — named on the command line, or the
     *  first row. */
    readonly property int openAt: catalog.openIndex

    onInspectorOpenChanged: settings.inspectorOpen = window.inspectorOpen
    // What the reader last set is written back as it changes, so the
    // window comes up on the browser they left.
    Connections {
        target: browser
        function onViewModeChanged() { settings.viewMode = browser.viewMode; }
        function onSortKeyChanged() { settings.sortKey = browser.sortKey; }
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
        view.sketchIndex = index;
    }

    // ---- Captures --------------------------------------------------------
    property string captureLine: ""
    Timer {
        id: captureHide

        interval: 2500
        onTriggered: window.captureLine = ""
    }
    function showCapture(path) {
        window.captureLine = path.length > 0
            ? "saved " + path.split("/").pop() : "capture failed";
        captureHide.restart();
    }

    FileDialog {
        id: videoDialog

        property int sketchIndex: -1
        fileMode: FileDialog.SaveFile
        nameFilters: ["MPEG-4 video (*.mp4)"]
        defaultSuffix: "mp4"
        acceptLabel: "Export"
        onAccepted: catalog.video(videoDialog.sketchIndex, selectedFile)
    }

    function exportVideo(index) {
        videoDialog.sketchIndex = index;
        videoDialog.currentFile = catalog.videoDefault(index);
        videoDialog.open();
    }

    Shortcut {
        // The plural form: Save is more than one binding on some
        // platforms, and binding the first silently drops the rest.
        sequences: [StandardKey.Save]
        onActivated: view.capture()
    }
    Shortcut {
        sequence: "/"
        onActivated: topBar.focusFilter()
    }
    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: topBar.focusFilter()
    }
    Shortcut {
        sequence: "Ctrl+I"
        onActivated: window.inspectorOpen = !window.inspectorOpen
    }

    // ---- Everything a sketch says about itself ----------------------------
    SketchCatalog { id: catalog }

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
            const learned = catalog.learn(
                stats.sketchIndex, stats.canvas ?? "",
                stats.moment ?? -1, stats.background ?? "",
                stats.runtime ?? "");
            if (learned.sketchIndex === undefined)
                return;
            browser.overlayRow(learned);
        }
        function onSketchIndexChanged() {
            browser.selectedIndex = view.sketchIndex;
        }
    }

    // A thumbnail landed, or a sketch has none and there is a line
    // saying why. The row is overlaid by index so exactly one card
    // changes — the reason learn() and the fill both route through here
    // rather than resetting the whole model, which would remount every
    // other thumbnail.
    Connections {
        target: catalog
        function onThumbnailReady(index, row) { browser.overlayRow(row); }
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
        function onThumbnailCaptured(index) { catalog.adoptThumbnail(index); }
    }

    Component.onCompleted: {
        browser.viewMode = settings.viewMode;
        window.inspectorOpen = settings.inspectorOpen;
        browser.sortKey = settings.sortKey;
        browser.sortAscending = settings.sortAscending;
        browser.openOn(window.openAt);
        // THE LOADING PHASE. Every sketch with no still gets one while
        // nothing is being presented, which is the only stretch in which
        // the machine is the fill's alone; opening a sketch ends it, and
        // the canvas opens by itself when it finishes.
        if (!catalog.openAtOnce)
            catalog.fillThumbnails();
        if (!catalog.filling)
            window.activate(window.openAt);
    }

    // ---- The window ------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar {
            id: topBar

            Layout.fillWidth: true
            total: catalog.sketches.length
            shown: browser.cards.length
            viewMode: browser.viewMode
            inspectorOpen: window.inspectorOpen
            taskRunning: catalog.taskRunning
            onFilterTextChanged: browser.filterText = topBar.filterText
            onViewModeRequested: mode => {
                browser.viewMode = mode;
                browser.reveal();
            }
            onInspectorToggled: window.inspectorOpen = !window.inspectorOpen
            onVideoRequested: window.exportVideo(-1)
            onSteppedOut: {
                if (browser.viewMode === "gallery")
                    gallery.focusRows();
                else
                    sketchList.focusRows();
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 5
                color: SplitHandle.pressed ? "#4a3d85"
                     : (SplitHandle.hovered ? "#2a2350" : Theme.rule)
            }

            // ---- The browser ----
            Rectangle {
                id: browserPane

                SplitView.preferredWidth: settings.browserWidth
                SplitView.minimumWidth: 330
                SplitView.maximumWidth: 1000
                color: Theme.panel

                // Remembered where it was LEFT, not wherever it passed
                // through: a pane takes several widths while a window
                // lays itself out, and any of those written down would
                // be the width it opened at next time.
                Component.onDestruction:
                    settings.browserWidth = browserPane.width

                SketchList {
                    id: sketchList

                    anchors.fill: parent
                    visible: browser.viewMode === "list"
                    catalog: catalog
                    rows: browser.rows
                    learnedSketches: browser.learnedSketches
                    selectedIndex: browser.selectedIndex
                    presentedIndex: view.sketchIndex
                    sortKey: browser.sortKey
                    sortAscending: browser.sortAscending
                    onSelectRequested: index => browser.select(index)
                    onActivateRequested: index => window.activate(index)
                    onGroupToggled: name => browser.toggleGroup(name)
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
                    folders: browser.folders
                    folder: browser.folder
                    selectedIndex: browser.selectedIndex
                    presentedIndex: view.sketchIndex
                    onSelectRequested: index => browser.select(index)
                    onActivateRequested: index => window.activate(index)
                    onFolderRequested: name => browser.folder = name
                    onStepRequested: delta => browser.step(delta)
                }
            }

            // ---- The canvas, which keeps presenting while you browse ----
            ColumnLayout {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 300
                spacing: 0

                Ui.PanZoomCanvas {
                    id: canvasViewport

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    canvasWidth: {
                        const dimensions = (view.metrics.canvas ?? "")
                            .split("x");
                        const value = Number(dimensions[0]);
                        return dimensions.length === 2 && value > 0
                            ? value : 900;
                    }
                    canvasHeight: {
                        const dimensions = (view.metrics.canvas ?? "")
                            .split("x");
                        const value = Number(dimensions[1]);
                        return dimensions.length === 2 && value > 0
                            ? value : 640;
                    }
                    checkerboardVisible: false
                    // The largest registry canvases still fit inside the
                    // common 16K texture limit at 4× on a Retina screen.
                    maximumScale: 4.0
                    panButtons: Qt.MiddleButton
                    showPanCursor: false
                    // A set sketch owns the ordinary wheel for camera
                    // distance. Ctrl-wheel still reaches zoom below.
                    mouseWheelZoomEnabled: !view.orbitable

                    SketchbookView {
                        id: view

                        anchors.fill: parent
                        // Captures run on the render thread; the saved path
                        // (or an empty string on failure) arrives
                        // asynchronously.
                        onCaptureReady: path => window.showCapture(path)

                        // Orbit, for the sketches that have a viewpoint to
                        // move. A drag is yaw and pitch; the wheel is
                        // distance. A sketch with no viewpoint gets no
                        // handler at all, so a drag over a drawn tree does
                        // nothing rather than something invisible.
                        //
                        // EVERY GESTURE STARTS FROM WHERE THE SKETCH STANDS,
                        // read off the view at the moment it begins, so an
                        // untouched sketch is seen from the camera it
                        // declared and the first drag moves that camera
                        // rather than replacing it.
                        property real yaw: 0
                        property real pitch: 0
                        property real distance: 0

                        DragHandler {
                            enabled: view.orbitable
                            target: null
                            property real startYaw: 0
                            property real startPitch: 0
                            onActiveChanged: {
                                if (active) {
                                    startYaw = view.orbitYaw;
                                    startPitch = view.orbitPitch;
                                    view.distance = view.orbitDistance;
                                }
                            }
                            onTranslationChanged: {
                                view.yaw = startYaw - translation.x * 0.4;
                                view.pitch = startPitch + translation.y * 0.3;
                                view.orbit(view.yaw, view.pitch, view.distance);
                            }
                        }
                        WheelHandler {
                            enabled: view.orbitable
                            onWheel: event => {
                                if (event.modifiers & Qt.ControlModifier) {
                                    const point = view.mapToItem(
                                        canvasViewport, event.x, event.y);
                                    const factor = Math.pow(
                                        1.4, event.angleDelta.y / 120.0);
                                    canvasViewport.zoomAt(
                                        factor, point.x, point.y);
                                    return;
                                }
                                view.yaw = view.orbitYaw;
                                view.pitch = view.orbitPitch;
                                view.distance = Math.max(
                                    40,
                                    view.orbitDistance
                                        - event.angleDelta.y * 0.5);
                                view.orbit(
                                    view.yaw, view.pitch, view.distance);
                            }
                        }

                        // The pointer and the keys, for the sketches that
                        // read them. Neither handler takes the grab, so the
                        // orbit above still drags a set; a sketch with
                        // nothing for a pointer to do ignores what arrives.
                        // The hover reports where the pointer stands with
                        // no button down, the point handler while one is,
                        // and a click gives this canvas the keyboard — so
                        // the keys go to the sketch after it is clicked and
                        // back to the list when the list is.
                        HoverHandler {
                            id: hover
                            onPointChanged: {
                                if (!press.active)
                                    view.pointer(point.position.x,
                                                 point.position.y, false);
                            }
                        }
                        PointHandler {
                            id: press
                            acceptedButtons: Qt.LeftButton
                            onActiveChanged: view.pointer(point.position.x,
                                                          point.position.y,
                                                          active)
                            onPointChanged: {
                                if (active)
                                    view.pointer(point.position.x,
                                                 point.position.y, true);
                            }
                        }
                        TapHandler {
                            onTapped: view.forceActiveFocus()
                        }
                        Keys.onPressed: event => {
                            view.key(event.key, event.text, true);
                            event.accepted = true;
                        }
                        Keys.onReleased: event => {
                            view.key(event.key, event.text, false);
                            event.accepted = true;
                        }
                    }
                }

                // Compile-error overlay: the last good sketch keeps
                // running underneath.
                Rectangle {
                    Layout.fillWidth: true
                    visible: view.errorLog.length > 0
                    color: "#2a0c12"
                    Layout.preferredHeight: Math.min(
                        errorText.implicitHeight + 20, window.height * 0.4)
                    ScrollView {
                        id: errorScroll

                        anchors.fill: parent
                        anchors.margins: 10
                        Text {
                            id: errorText

                            text: view.errorLog
                            color: Theme.bad
                            font.family: Theme.mono
                            font.pixelSize: 12
                            textFormat: Text.PlainText
                            wrapMode: Text.WrapAnywhere
                            width: errorScroll.width - 20
                        }
                    }
                }
            }

            // ---- The inspector ----
            Inspector {
                id: inspector

                SplitView.preferredWidth: 350
                SplitView.minimumWidth: 280
                SplitView.maximumWidth: 520
                visible: window.inspectorOpen
                catalog: catalog
                sketch: browser.selectedSketch
                presented: browser.selectedIndex === view.sketchIndex
                metrics: view.metrics
                taskLine: catalog.taskLine
                taskRunning: catalog.taskRunning
                onOpenRequested: window.activate(browser.selectedIndex)
                onFrameRequested: catalog.frame(browser.selectedIndex)
                onVideoRequested: window.exportVideo(browser.selectedIndex)
                onBenchRequested: catalog.bench(browser.selectedIndex)
                onRevealRequested: catalog.reveal(browser.selectedIndex)
            }
        }

        StatusStrip {
            Layout.fillWidth: true
            hostState: view.state
            status: view.status
            filling: catalog.filling
            fillDone: catalog.fillDone
            fillTotal: catalog.fillTotal
            fillNote: catalog.fillNote
            sketch: view.metrics.sketch ?? ""
            path: browser.sketchAt(view.sketchIndex)?.path ?? ""
            hints: "↑↓ select · ⏎ open · / filter"
                + (view.orbitable
                    ? " · drag/wheel orbit · ctrl-wheel zoom"
                    : " · wheel zoom · middle-drag pan")
                + (view.activeFocus ? " · keys go to the sketch" : "")
            paused: view.paused
            timeScale: view.timeScale
            metrics: view.metrics
            capture: window.captureLine
            onPauseToggled: view.paused = !view.paused
            onCaptureRequested: view.capture()
            onTimeScaleRequested: scale => view.timeScale = scale
        }
    }
}
