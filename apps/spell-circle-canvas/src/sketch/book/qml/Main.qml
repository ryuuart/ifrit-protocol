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

    // ---- The browser's state ---------------------------------------------
    property string viewMode: "list"
    property bool inspectorOpen: true
    property string sortKey: "folder"
    property bool sortAscending: true
    property string filterText: ""
    property string folder: ""
    /** The row the inspector shows. Not the row the canvas presents:
     *  that is view.sketchIndex, and the two part company the moment
     *  someone starts browsing. */
    property int selectedIndex: -1
    property var collapsedGroups: ({})
    property var rows: []
    property var cards: []
    property var folders: []
    /** Session-only facts keyed by registry index. They overlay the stable
     *  browser models so learning one canvas does not remount every thumbnail. */
    property var learnedSketches: ({})
    property int rebuildGeneration: 0

    onViewModeChanged: settings.viewMode = window.viewMode
    onInspectorOpenChanged: settings.inspectorOpen = window.inspectorOpen
    onSortKeyChanged: {
        settings.sortKey = window.sortKey;
        window.rebuild();
    }
    onSortAscendingChanged: {
        settings.sortAscending = window.sortAscending;
        window.rebuild();
    }
    onFilterTextChanged: window.rebuild()
    onFolderChanged: window.rebuild()

    /** Every field a row carries, empty — what a folder header stands
     *  in with, so a delegate never reads a field off the wrong kind of
     *  row and warns once per row per rebuild for nothing. */
    readonly property var blankSketch: ({
        sketchIndex: -1, name: "", key: "", folder: "", blurb: "", path: "",
        kind: "", available: true, reason: "", lines: 0, subject: "",
        editFirst: "", plate: "", canvas: "", background: "", moment: -1,
        videoExportable: false
    })

    readonly property var selectedSketch:
        window.learnedSketches[window.selectedIndex]
            ?? window.sketchAt(window.selectedIndex) ?? window.blankSketch

    function sketchAt(index) {
        const all = catalog.sketches;
        for (let i = 0; i < all.length; ++i)
            if (all[i].sketchIndex === index)
                return all[i];
        return undefined;
    }

    // ---- Filtering -------------------------------------------------------
    //
    // Free words narrow on everything a sketch is written down as; a
    // `folder:` or `kind:` word narrows on that field alone. Every word
    // has to match, so words accumulate into one question rather than
    // widening it.

    function parseFilter(text) {
        let terms = { free: [], folder: [], kind: [] };
        const words = text.trim().toLowerCase().split(/\s+/);
        for (let i = 0; i < words.length; ++i) {
            const word = words[i];
            if (word.length === 0)
                continue;
            if (word.startsWith("folder:"))
                terms.folder.push(word.substring(7));
            else if (word.startsWith("kind:"))
                terms.kind.push(word.substring(5));
            else
                terms.free.push(word);
        }
        return terms;
    }

    function matches(sketch, terms) {
        const hay = (sketch.name + " " + sketch.folder + " " + sketch.blurb
                     + " " + sketch.key).toLowerCase();
        for (let i = 0; i < terms.free.length; ++i)
            if (hay.indexOf(terms.free[i]) < 0)
                return false;
        for (let i = 0; i < terms.folder.length; ++i)
            if (sketch.folder.toLowerCase().indexOf(terms.folder[i]) < 0)
                return false;
        for (let i = 0; i < terms.kind.length; ++i)
            if (sketch.kind.toLowerCase().indexOf(terms.kind[i]) < 0)
                return false;
        return true;
    }

    // ---- Ordering --------------------------------------------------------
    //
    // Ordering by FOLDER is the grouped reading, and is the default; any
    // other column is one flat run over the whole filtered set, because
    // a column you asked to be ordered by is one you want to read down
    // without folders interrupting it.

    function sortValue(sketch, key) {
        if (key === "folder")
            return sketch.folder + " " + sketch.name;
        if (key === "kind")
            return sketch.kind + " " + sketch.name;
        if (key === "lines")
            return sketch.lines;
        if (key === "moment")
            return sketch.moment;
        if (key === "canvas") {
            const size = /^(\d+)x(\d+)$/.exec(sketch.canvas);
            return size ? Number(size[1]) * Number(size[2]) : -1;
        }
        return sketch.name;
    }

    function compare(left, right) {
        const a = window.sortValue(left, window.sortKey);
        const b = window.sortValue(right, window.sortKey);
        // A fact a session has not answered yet sorts to the end either
        // way: it is not a small number, it is an unknown one.
        if (typeof a === "number" && typeof b === "number") {
            if (a < 0 && b >= 0) return 1;
            if (b < 0 && a >= 0) return -1;
            return window.sortAscending ? a - b : b - a;
        }
        return window.sortAscending ? a.localeCompare(b) : b.localeCompare(a);
    }

    // ---- The rows --------------------------------------------------------

    function rebuild() {
        // Replacing either JavaScript-array model makes its view choose a
        // fresh contentY. Keep the reader's place across facts learned from
        // a newly presented sketch (and across any other registry rebuild).
        const listScroll = sketchList.scrollPosition();
        const galleryScroll = gallery.scrollPosition();
        const generation = ++window.rebuildGeneration;
        const terms = window.parseFilter(window.filterText);
        const all = catalog.sketches;
        let found = [];
        for (let i = 0; i < all.length; ++i)
            if (window.matches(all[i], terms))
                found.push(all[i]);

        // The chips count what the TEXT left, so a chip says how many
        // there are to switch to rather than how many are on screen.
        let counts = ({});
        let order = [];
        for (let i = 0; i < found.length; ++i) {
            if (counts[found[i].folder] === undefined) {
                counts[found[i].folder] = 0;
                order.push(found[i].folder);
            }
            ++counts[found[i].folder];
        }
        order.sort();
        let chips = [{ folder: "", label: "All", count: found.length }];
        for (let i = 0; i < order.length; ++i)
            chips.push({ folder: order[i], label: order[i],
                         count: counts[order[i]] });
        window.folders = chips;

        let kept = [];
        for (let i = 0; i < found.length; ++i)
            if (window.folder.length === 0 || found[i].folder === window.folder)
                kept.push(found[i]);
        kept.sort(window.compare);
        window.cards = kept;

        let out = [];
        if (window.sortKey === "folder") {
            let openFolder = "";
            let shut = false;
            for (let i = 0; i < kept.length; ++i) {
                if (kept[i].folder !== openFolder) {
                    openFolder = kept[i].folder;
                    // While filtering, a collapsed folder would hide its
                    // own hits — which is worse than no filter at all.
                    shut = terms.free.length === 0
                        && terms.folder.length === 0
                        && terms.kind.length === 0
                        && window.collapsedGroups[openFolder] === true;
                    let count = 0;
                    for (let k = i; k < kept.length
                         && kept[k].folder === openFolder; ++k)
                        ++count;
                    out.push({ header: true, folder: openFolder, count: count,
                               collapsed: shut, sketch: window.blankSketch });
                }
                if (!shut)
                    out.push({ header: false, folder: kept[i].folder,
                               count: 0, collapsed: false, sketch: kept[i] });
            }
        } else {
            for (let i = 0; i < kept.length; ++i)
                out.push({ header: false, folder: kept[i].folder, count: 0,
                           collapsed: false, sketch: kept[i] });
        }
        window.rows = out;

        // A selection nothing on screen shows is not a selection: it
        // falls to the first row there is, which is the first row of the
        // first OPEN folder rather than of the first folder.
        if (window.rowForSketch(window.selectedIndex) < 0) {
            window.selectedIndex = -1;
            const showing = window.viewMode === "gallery" ? kept : out;
            for (let i = 0; i < showing.length; ++i) {
                const row = showing[i].header === undefined
                    ? showing[i] : (showing[i].header ? null : showing[i].sketch);
                if (row !== null) {
                    window.selectedIndex = row.sketchIndex;
                    break;
                }
            }
        }

        // The views apply their new models on the next event-loop turn.
        // Only the latest rebuild gets to put the saved positions back.
        Qt.callLater(function() {
            if (generation !== window.rebuildGeneration)
                return;
            sketchList.restoreScrollPosition(listScroll);
            gallery.restoreScrollPosition(galleryScroll);
        });
    }

    function toggleGroup(name) {
        let next = ({});
        for (const key in window.collapsedGroups)
            next[key] = window.collapsedGroups[key];
        next[name] = !(next[name] === true);
        window.collapsedGroups = next;
        window.rebuild();
    }

    /** Where the selected sketch sits in whichever view is on, or -1
     *  when this one is not showing it. */
    function rowForSketch(index) {
        if (window.viewMode === "gallery") {
            for (let i = 0; i < window.cards.length; ++i)
                if (window.cards[i].sketchIndex === index)
                    return i;
            return -1;
        }
        for (let i = 0; i < window.rows.length; ++i)
            if (!window.rows[i].header
                && window.rows[i].sketch.sketchIndex === index)
                return i;
        return -1;
    }

    /** Moves the selection by @p step over what is actually on screen,
     *  stepping over folder headers, and scrolls it into view. */
    function step(delta) {
        const here = window.rowForSketch(window.selectedIndex);
        if (window.viewMode === "gallery") {
            if (window.cards.length === 0)
                return;
            const to = Math.max(0, Math.min(window.cards.length - 1,
                                            (here < 0 ? 0 : here + delta)));
            window.selectedIndex = window.cards[to].sketchIndex;
            gallery.positionAt(to);
            return;
        }
        let i = here >= 0 ? here : (delta > 0 ? -1 : window.rows.length);
        for (i += delta; i >= 0 && i < window.rows.length; i += delta) {
            if (window.rows[i].header)
                continue;
            window.selectedIndex = window.rows[i].sketch.sketchIndex;
            sketchList.positionAt(i);
            return;
        }
    }

    function select(index) {
        window.selectedIndex = index;
    }

    /** Present it. The one action that moves the canvas. */
    function activate(index) {
        if (index >= 0)
            view.sketchIndex = index;
    }

    /** Brings the selection into view — opening the folder holding it
     *  first, because a selection inside a shut folder is one the list
     *  cannot show and the next rebuild would move.
     *
     *  The scroll waits for the rows to have been handed over: a view
     *  asked to hold an index it has not been given yet holds nothing,
     *  and the selection stays off screen with no sign that it did. */
    function reveal() {
        const sketch = window.sketchAt(window.selectedIndex);
        if (sketch === undefined)
            return;
        if (window.viewMode === "list"
            && window.collapsedGroups[sketch.folder] === true)
            window.toggleGroup(sketch.folder);
        Qt.callLater(window.scrollToSelection);
    }

    function scrollToSelection() {
        const at = window.rowForSketch(window.selectedIndex);
        if (at < 0)
            return;
        if (window.viewMode === "gallery")
            gallery.positionAt(at);
        else
            sketchList.positionAt(at);
    }

    function sortBy(key) {
        if (window.sortKey === key)
            window.sortAscending = !window.sortAscending;
        else {
            window.sortAscending = true;
            window.sortKey = key;
        }
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
            window.overlayRow(learned);
        }
        function onSketchIndexChanged() {
            window.selectedIndex = view.sketchIndex;
        }
    }

    // A thumbnail landed, or could not be drawn. The row is overlaid by
    // index so exactly one card changes — the reason learn() and the
    // thumbnail worker both route through here rather than resetting the
    // whole model, which would remount every other thumbnail.
    Connections {
        target: catalog
        function onThumbnailReady(index, row) { window.overlayRow(row); }
        function onThumbnailFailed(name) {
            window.captureLine = "thumbnail failed — " + name;
            captureHide.restart();
        }
    }

    /** Overlays one row by its sketch index without disturbing the rest. */
    function overlayRow(row) {
        if (row.sketchIndex === undefined)
            return;
        let next = ({});
        for (const index in window.learnedSketches)
            next[index] = window.learnedSketches[index];
        next[row.sketchIndex] = row;
        window.learnedSketches = next;
    }

    Component.onCompleted: {
        window.viewMode = settings.viewMode;
        window.inspectorOpen = settings.inspectorOpen;
        window.sortKey = settings.sortKey;
        window.sortAscending = settings.sortAscending;
        // Open on the SHAPE of the registry rather than on its first
        // thirteen rows: every folder shut but the one holding what the
        // canvas is presenting is one screen that says what is here.
        const all = catalog.sketches;
        const current = window.sketchAt(view.sketchIndex);
        let next = ({});
        for (let i = 0; i < all.length; ++i)
            next[all[i].folder] =
                current === undefined || all[i].folder !== current.folder;
        window.collapsedGroups = next;
        window.selectedIndex = view.sketchIndex;
        window.rebuild();
        window.reveal();
    }

    // ---- The window ------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar {
            id: topBar

            Layout.fillWidth: true
            total: catalog.sketches.length
            shown: window.cards.length
            viewMode: window.viewMode
            inspectorOpen: window.inspectorOpen
            taskRunning: catalog.taskRunning
            onFilterTextChanged: window.filterText = topBar.filterText
            onViewModeRequested: mode => {
                window.viewMode = mode;
                window.reveal();
            }
            onInspectorToggled: window.inspectorOpen = !window.inspectorOpen
            onVideoRequested: window.exportVideo(-1)
            onSteppedOut: {
                if (window.viewMode === "gallery")
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
                    visible: window.viewMode === "list"
                    catalog: catalog
                    rows: window.rows
                    learnedSketches: window.learnedSketches
                    selectedIndex: window.selectedIndex
                    presentedIndex: view.sketchIndex
                    sortKey: window.sortKey
                    sortAscending: window.sortAscending
                    onSelectRequested: index => window.select(index)
                    onActivateRequested: index => window.activate(index)
                    onGroupToggled: name => window.toggleGroup(name)
                    onSortRequested: key => window.sortBy(key)
                    onStepRequested: delta => window.step(delta)
                }

                SketchGallery {
                    id: gallery

                    anchors.fill: parent
                    visible: window.viewMode === "gallery"
                    catalog: catalog
                    cards: window.cards
                    learnedSketches: window.learnedSketches
                    folders: window.folders
                    folder: window.folder
                    selectedIndex: window.selectedIndex
                    presentedIndex: view.sketchIndex
                    onSelectRequested: index => window.select(index)
                    onActivateRequested: index => window.activate(index)
                    onFolderRequested: name => window.folder = name
                    onStepRequested: delta => window.step(delta)
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
                sketch: window.selectedSketch
                presented: window.selectedIndex === view.sketchIndex
                metrics: view.metrics
                taskLine: catalog.taskLine
                taskRunning: catalog.taskRunning
                onOpenRequested: window.activate(window.selectedIndex)
                onFrameRequested: catalog.frame(window.selectedIndex)
                onVideoRequested: window.exportVideo(window.selectedIndex)
                onBenchRequested: catalog.bench(window.selectedIndex)
                onRevealRequested: catalog.reveal(window.selectedIndex)
            }
        }

        StatusStrip {
            Layout.fillWidth: true
            hostState: view.state
            status: view.status
            sketch: view.metrics.sketch ?? ""
            path: window.sketchAt(view.sketchIndex)?.path ?? ""
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
