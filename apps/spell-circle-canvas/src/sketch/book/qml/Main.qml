// Delegates reach outward — a row needs the window's row array, the list's
// width, and the view's current sketch. Bound makes those captures explicit
// and well-defined rather than resolved by scope-chain accident.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sigil.Sketchbook

ApplicationWindow {
    id: window

    width: 1440
    height: 920
    minimumWidth: 760
    minimumHeight: 560
    visible: true
    title: "Sketchbook"
    color: "#0b0a14"

    // The Basic style paints its controls straight from the palette; without
    // this a light-grey Button and Slider sit in the middle of a dark panel.
    palette.window: "#12101e"
    palette.windowText: "#e8ecf8"
    palette.button: "#241f3d"
    palette.buttonText: "#e8ecf8"
    palette.mid: "#2f2951"
    palette.midlight: "#3a3366"
    palette.dark: "#0a0912"
    palette.light: "#3a3366"
    palette.highlight: "#5b4bb0"
    palette.text: "#e8ecf8"
    palette.base: "#0a0912"

    readonly property var stats: view.metrics

    function showCapture(path) {
        captureLabel.text = path.length > 0
            ? "saved " + path.split("/").pop() : "capture failed";
        captureLabel.opacity = 1;
        captureHide.restart();
    }
    Timer {
        id: captureHide
        interval: 2500
        onTriggered: captureLabel.opacity = 0
    }
    Shortcut {
        // The plural form: Save is more than one binding on some
        // platforms, and binding the first silently drops the rest.
        sequences: [StandardKey.Save]
        onActivated: view.capture()
    }

    // ---- Sidebar navigation model -----------------------------------------
    //
    // The registry is more entries than fit on screen, so the sidebar is
    // FOLDERS — one group per category, collapsible, over a single flat row
    // array that mixes headers and sketches. A flat array (rather than a
    // nested view) keeps one ListView, one currentIndex, and working arrow
    // keys.
    //
    // Filtering is the other half. Typing narrows on name, blurb, folder and
    // file stem, and force-opens every group that still has a hit — a search
    // that leaves matches hidden inside a collapsed folder is worse than no
    // search.

    property string filterText: ""
    property var collapsedGroups: ({})
    property var rows: []

    function matches(sketch, needle) {
        if (needle.length === 0)
            return true;
        return (sketch.name + " " + sketch.tag + " " + sketch.category + " "
                + sketch.key).toLowerCase().indexOf(needle) >= 0;
    }

    function rebuildRows() {
        const needle = filterText.trim().toLowerCase();
        const all = view.sketches;
        let order = [];
        let byGroup = ({});
        for (let i = 0; i < all.length; ++i) {
            const sketch = all[i];
            if (!matches(sketch, needle))
                continue;
            if (byGroup[sketch.category] === undefined) {
                byGroup[sketch.category] = [];
                order.push(sketch.category);
            }
            byGroup[sketch.category].push(sketch);
        }
        // Headers and sketches get the SAME shape. A delegate reading a field
        // the other kind lacks would evaluate `undefined` on every row of the
        // wrong kind — a warning per row per rebuild, for nothing.
        let out = [];
        for (let g = 0; g < order.length; ++g) {
            const group = order[g];
            const items = byGroup[group];
            // While filtering, a collapsed folder would hide its own hits.
            const shut = needle.length === 0
                         && collapsedGroups[group] === true;
            out.push({ header: true, name: group, tag: "", key: "", path: "",
                       sketchIndex: -1, count: items.length, collapsed: shut });
            if (!shut)
                for (let k = 0; k < items.length; ++k)
                    out.push({ header: false, name: items[k].name,
                               tag: items[k].tag, key: items[k].key,
                               path: items[k].path,
                               sketchIndex: items[k].sketchIndex,
                               count: 0, collapsed: false });
        }
        rows = out;
    }

    function toggleGroup(group) {
        let next = ({});
        for (const key in collapsedGroups)
            next[key] = collapsedGroups[key];
        next[group] = !(next[group] === true);
        collapsedGroups = next;
        rebuildRows();
    }

    function setAllCollapsed(shut) {
        let next = ({});
        const all = view.sketches;
        for (let i = 0; i < all.length; ++i)
            next[all[i].category] = shut;
        collapsedGroups = next;
        rebuildRows();
    }

    /** The row showing `sketchIndex`, or -1 when the filter hides it. */
    function rowForSketch(sketchIndex) {
        for (let i = 0; i < rows.length; ++i)
            if (!rows[i].header && rows[i].sketchIndex === sketchIndex)
                return i;
        return -1;
    }

    /** Opens the folder holding the running sketch and scrolls to it. The
     *  selection can change from outside the list, and a selection you
     *  cannot see is not a selection. */
    function revealCurrent() {
        const sketch = view.sketches[view.sketchIndex];
        if (sketch === undefined)
            return;
        if (collapsedGroups[sketch.category] === true)
            toggleGroup(sketch.category);
        const rowIndex = rowForSketch(view.sketchIndex);
        if (rowIndex >= 0)
            sketchList.positionViewAtIndex(rowIndex, ListView.Contain);
    }

    Connections {
        target: view
        function onSketchIndexChanged() { window.revealCurrent(); }
    }

    /** Arrow keys move between SKETCHES, stepping over folder headers — and
     *  land somewhere sensible when the current one is filtered out. */
    function selectDelta(step) {
        const start = rowForSketch(view.sketchIndex);
        let i = start >= 0 ? start : (step > 0 ? -1 : rows.length);
        for (i += step; i >= 0 && i < rows.length; i += step) {
            if (!rows[i].header) {
                view.sketchIndex = rows[i].sketchIndex;
                sketchList.positionViewAtIndex(i, ListView.Contain);
                return;
            }
        }
    }

    onFilterTextChanged: rebuildRows()

    // Open on the SHAPE of the registry, not on its first thirteen rows. The
    // whole list expanded is several screens of scrolling before you have
    // seen that a folder exists; the headers with the running sketch's folder
    // open is one screen that says what is here.
    Component.onCompleted: {
        const all = view.sketches;
        const current = all[view.sketchIndex];
        let next = ({});
        for (let i = 0; i < all.length; ++i)
            next[all[i].category] =
                current === undefined || all[i].category !== current.category;
        collapsedGroups = next;
        rebuildRows();
        revealCurrent();
    }

    /** One "name   value" line that gives up width by eliding the name
     *  rather than by painting past the panel. */
    component Metric: RowLayout {
        id: metricRow

        required property string name
        required property string value

        spacing: 6
        Label {
            text: metricRow.name
            color: "#767e99"
            font.pixelSize: 11
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Label {
            text: metricRow.value
            color: "#d3dbf0"
            font.family: "Menlo"
            font.pixelSize: 11
        }
    }

    // Draggable split: the blurbs are long and the metric names are fixed, so
    // how much of the window the sidebar deserves is the reader's call.
    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        handle: Rectangle {
            implicitWidth: 5
            color: SplitHandle.pressed ? "#4a3d85"
                                       : (SplitHandle.hovered ? "#2a2350"
                                                              : "#181530")
        }

        // ---- Sidebar: the registry, grouped by folder ----
        Rectangle {
            SplitView.preferredWidth: 320
            SplitView.minimumWidth: 240
            SplitView.maximumWidth: 520
            color: "#12101e"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label {
                        text: "SKETCHBOOK"
                        color: "#ffb46b"
                        font.pixelSize: 15
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: view.sketches.length + " sketches"
                        color: "#5c6480"
                        font.family: "Menlo"
                        font.pixelSize: 10
                    }
                }

                // ---- Filter ----
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    radius: 8
                    color: "#0a0912"
                    border.width: 1
                    border.color: filterField.activeFocus ? "#5b4bb0" : "#1e1a33"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 9
                        anchors.rightMargin: 5
                        spacing: 4

                        Label {
                            text: "⌕"
                            color: "#5c6480"
                            font.pixelSize: 14
                        }
                        TextField {
                            id: filterField

                            Layout.fillWidth: true
                            placeholderText: "filter — name, folder, blurb, file"
                            color: "#e8ecf8"
                            placeholderTextColor: "#4d5470"
                            font.pixelSize: 12
                            background: null
                            padding: 0
                            onTextChanged: window.filterText = text
                            // Escape clears rather than losing focus: the
                            // filter is a lens, and putting it down should be
                            // one key, not select-all-delete.
                            Keys.onEscapePressed: text = ""
                            Keys.onDownPressed: {
                                sketchList.forceActiveFocus();
                                window.selectDelta(1);
                            }
                        }
                        ToolButton {
                            visible: filterField.text.length > 0
                            text: "×"
                            font.pixelSize: 15
                            implicitWidth: 22
                            implicitHeight: 22
                            onClicked: filterField.text = ""
                        }
                        ToolButton {
                            // One click to see the whole shape of the
                            // registry, one to get back to browsing.
                            visible: filterField.text.length === 0
                            text: "≡"
                            font.pixelSize: 14
                            implicitWidth: 22
                            implicitHeight: 22
                            ToolTip.visible: hovered
                            ToolTip.delay: 600
                            ToolTip.text: "Collapse / expand all folders"
                            onClicked: {
                                let anyOpen = false;
                                for (let i = 0; i < window.rows.length; ++i)
                                    if (window.rows[i].header
                                        && !window.rows[i].collapsed)
                                        anyOpen = true;
                                window.setAllCollapsed(anyOpen);
                            }
                        }
                    }
                }

                ListView {
                    id: sketchList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // The metrics panel below is fixed-height; without a floor
                    // here a short window would collapse the list to nothing.
                    Layout.minimumHeight: 120
                    clip: true
                    focus: true
                    model: window.rows
                    // The list is folded and filtered, so a row's position
                    // says nothing about which sketch it selects — the mapping
                    // runs the other way, and is -1 when the filter has hidden
                    // the running one.
                    currentIndex: window.rowForSketch(view.sketchIndex)
                    keyNavigationEnabled: false
                    Keys.onUpPressed: window.selectDelta(-1)
                    Keys.onDownPressed: window.selectDelta(1)

                    ScrollBar.vertical: ScrollBar { id: sketchScroll }

                    readonly property real rowWidth:
                        sketchList.width
                        - (sketchScroll.visible ? sketchScroll.width : 0)

                    // One delegate, two faces. A Loader per row would have to
                    // hand the row down through a required property after
                    // construction, which is exactly what required properties
                    // forbid; cheap Items cost less than that fight.
                    delegate: Item {
                        id: row

                        required property var modelData

                        width: sketchList.rowWidth
                        height: row.modelData.header ? 28 : 42

                        // ---- A folder ----
                        Rectangle {
                            anchors.fill: parent
                            visible: row.modelData.header
                            radius: 6
                            color: headerHover.hovered ? "#181530"
                                                       : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 8
                                spacing: 5

                                Label {
                                    // Fixed width so every folder name starts
                                    // at the same x whichever way it points.
                                    Layout.preferredWidth: 10
                                    horizontalAlignment: Text.AlignHCenter
                                    text: row.modelData.collapsed ? "▶"
                                                                  : "▼"
                                    color: "#79839f"
                                    font.pixelSize: 8
                                }
                                Label {
                                    text: row.modelData.name
                                    color: "#8f98b2"
                                    font.pixelSize: 11
                                    font.capitalization: Font.AllUppercase
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: row.modelData.count
                                    color: "#4d5470"
                                    font.family: "Menlo"
                                    font.pixelSize: 10
                                }
                            }

                            HoverHandler { id: headerHover }
                            TapHandler {
                                onTapped: {
                                    window.toggleGroup(row.modelData.name);
                                    sketchList.forceActiveFocus();
                                }
                            }
                        }

                        // ---- A sketch ----
                        // Two lines, both elided: blurbs run well past what a
                        // single-row layout can hold beside the name.
                        Rectangle {
                            anchors.fill: parent
                            visible: !row.modelData.header
                            radius: 7
                            color: view.sketchIndex === row.modelData.sketchIndex
                                 ? "#2c2456"
                                 : sketchHover.hovered ? "#1b1730" : "transparent"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 21
                                anchors.rightMargin: 10
                                anchors.topMargin: 5
                                anchors.bottomMargin: 5
                                spacing: 1

                                Label {
                                    text: row.modelData.name
                                    color: "#e8ecf8"
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: row.modelData.tag
                                    color: "#6fc9e0"
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            HoverHandler { id: sketchHover }
                            TapHandler {
                                onTapped: {
                                    view.sketchIndex = row.modelData.sketchIndex;
                                    sketchList.forceActiveFocus();
                                }
                            }
                            // Every sketch says where it lives: the file you
                            // would open to change what you are looking at.
                            ToolTip.visible: sketchHover.hovered
                            ToolTip.delay: 700
                            ToolTip.text: row.modelData.name + " — "
                                + row.modelData.tag + "\n"
                                + row.modelData.path
                        }
                    }
                }

                // ---- Clock controls ----
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Button {
                        text: view.paused ? "Resume" : "Pause"
                        Layout.fillWidth: true
                        onClicked: view.paused = !view.paused
                    }
                    Button {
                        text: "Capture"
                        onClicked: view.capture()
                    }
                    Label {
                        text: speed.value.toFixed(2) + "×"
                        color: "#767e99"
                        font.family: "Menlo"
                        font.pixelSize: 11
                    }
                }
                Slider {
                    id: speed
                    Layout.fillWidth: true
                    from: 0.1
                    to: 4.0
                    value: 1.0
                    onValueChanged: view.timeScale = value
                }
                Label {
                    id: captureLabel
                    color: "#5bd47a"
                    font.pixelSize: 11
                    opacity: 0
                    Behavior on opacity { NumberAnimation { duration: 300 } }
                }

                // ---- Live frame metrics ----
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: metricsBody.implicitHeight + 24
                    Layout.minimumHeight: metricsBody.implicitHeight + 24
                    radius: 10
                    color: "#0a0912"
                    border.width: 1
                    border.color: "#1e1a33"

                    ColumnLayout {
                        id: metricsBody

                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Label {
                                text: window.stats.fps !== undefined
                                      ? window.stats.fps.toFixed(0) : "—"
                                color: "#7ee8ff"
                                font.pixelSize: 30
                                font.bold: true
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Label {
                                    text: "fps presented"
                                    color: "#767e99"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: window.stats.backend ?? ""
                                    color: "#ffb46b"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 18
                            rowSpacing: 3

                            Metric {
                                Layout.fillWidth: true
                                name: "work"
                                value: (window.stats.workMs ?? 0).toFixed(2)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "p99"
                                value: (window.stats.p99Ms ?? 0).toFixed(2)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "submit"
                                value: (window.stats.submitMs ?? 0).toFixed(2)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "headroom"
                                value: (window.stats.headroomFps ?? 0).toFixed(0)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "canvas"
                                value: window.stats.canvas ?? "—"
                            }
                        }

                        // The runtime's own lanes, named by the runtime: a
                        // drawn tree and a lit set do not spend a frame on
                        // the same things, and pretending otherwise would
                        // print zeros under a heading one of them cannot
                        // fill.
                        Repeater {
                            model: window.stats.lanes ?? []
                            Metric {
                                required property var modelData
                                Layout.fillWidth: true
                                name: modelData.name
                                value: modelData.ms.toFixed(2)
                            }
                        }

                        Label {
                            text: window.stats.counters ?? ""
                            visible: text.length > 0
                            color: "#767e99"
                            font.family: "Menlo"
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        // ---- The sketch surface, with the live host's own status under it ----
        ColumnLayout {
            SplitView.fillWidth: true
            spacing: 0

            SketchbookView {
                id: view
                Layout.fillWidth: true
                Layout.fillHeight: true
                // Captures run on the render thread; the saved path (or an
                // empty string on failure) arrives asynchronously.
                onCaptureReady: path => window.showCapture(path)

                // Orbit, for the sketches that have a viewpoint to move. A
                // drag is yaw and pitch; the wheel is distance. A sketch with
                // no viewpoint gets no handler at all, so a drag over a drawn
                // tree does nothing rather than something invisible.
                //
                // EVERY GESTURE STARTS FROM WHERE THE SKETCH STANDS, read off
                // the view at the moment it begins, so an untouched sketch is
                // seen from the camera it declared and the first drag moves
                // that camera rather than replacing it.
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
                        view.yaw = view.orbitYaw;
                        view.pitch = view.orbitPitch;
                        view.distance = Math.max(
                            40, view.orbitDistance - event.angleDelta.y * 0.5);
                        view.orbit(view.yaw, view.pitch, view.distance);
                    }
                }
            }

            // Compile-error overlay: the last good sketch keeps running
            // underneath.
            Rectangle {
                Layout.fillWidth: true
                visible: view.errorLog.length > 0
                color: "#2a0c12"
                Layout.preferredHeight: Math.min(
                    errorText.implicitHeight + 20, window.height * 0.4)
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 10
                    Text {
                        id: errorText
                        text: view.errorLog
                        color: "#ff9aa4"
                        font.family: "Menlo"
                        font.pixelSize: 12
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                        width: window.width - 20
                    }
                }
            }

            // Status bar: state dot + build status.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                color: "#12101e"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 8

                    // Green live, amber compiling (pulsing), red failed,
                    // grey waiting.
                    Rectangle {
                        id: stateDot
                        width: 10
                        height: 10
                        radius: 5
                        color: view.state === "live" ? "#5bd47a"
                             : view.state === "compiling" ? "#ffb46b"
                             : view.state === "failed" ? "#ff5a6e"
                             : "#5a5f73"
                        SequentialAnimation on opacity {
                            running: view.state === "compiling"
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.25; duration: 350 }
                            NumberAnimation { to: 1.0; duration: 350 }
                            onRunningChanged: if (!running) stateDot.opacity = 1
                        }
                    }
                    // WHAT IS RUNNING, beside how it is doing. The sidebar
                    // says it too, but a list long enough to scroll can put
                    // the selected row off screen, and the one line that
                    // never moves is the one worth naming it on.
                    Label {
                        text: window.stats.sketch !== undefined
                              ? window.stats.sketch : ""
                        color: "#e8ecf8"
                        font.pixelSize: 12
                        visible: text.length > 0
                    }
                    Label {
                        text: view.status
                        color: view.state === "failed" ? "#ff9aa4"
                             : view.state === "compiling" ? "#ffd9a0"
                             : "#7ee8ff"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: view.orbitable ? "drag to orbit · save to reload"
                                             : "save to reload"
                        color: "#5a5f73"
                        font.pixelSize: 12
                    }
                }
            }
        }
    }
}
