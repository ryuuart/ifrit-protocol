// Delegates reach outward — a row needs the window's row array, the list's
// width, and the view's current scene. Bound makes those captures explicit
// and well-defined rather than resolved by scope-chain accident.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Compose.Gallery

ApplicationWindow {
    id: window

    width: 1440
    height: 920
    minimumWidth: 760
    minimumHeight: 560
    visible: true
    title: "ComposeGallery — SigilCompose stress catalog"
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

    // ---- Sidebar navigation model -----------------------------------------
    //
    // The registry outgrew a flat list: the catalog scenes and the study
    // sketches together are more entries than fit on screen, and they are not
    // one kind of thing. So the sidebar is FOLDERS — one group per category,
    // collapsible, over a single flat row array that mixes headers and
    // scenes. A flat array (rather than a nested view) keeps one ListView,
    // one currentIndex, and working arrow keys.
    //
    // Filtering is the other half. Typing narrows on name, blurb, folder and
    // a study's file stem, and force-opens every group that still has a hit —
    // a search that leaves matches hidden inside a collapsed folder is worse
    // than no search.

    property string filterText: ""
    property var collapsedGroups: ({})
    property var rows: []

    function matches(scene, needle) {
        if (needle.length === 0)
            return true;
        return (scene.name + " " + scene.tag + " " + scene.category + " "
                + scene.key).toLowerCase().indexOf(needle) >= 0;
    }

    function rebuildRows() {
        const needle = filterText.trim().toLowerCase();
        const all = view.scenes;
        let order = [];
        let byGroup = ({});
        for (let i = 0; i < all.length; ++i) {
            const scene = all[i];
            if (!matches(scene, needle))
                continue;
            if (byGroup[scene.category] === undefined) {
                byGroup[scene.category] = [];
                order.push(scene.category);
            }
            byGroup[scene.category].push(scene);
        }
        // Headers and scenes get the SAME shape. A delegate reading a field
        // the other kind lacks would evaluate `undefined` on every row of the
        // wrong kind — a warning per row per rebuild, for nothing.
        let out = [];
        for (let g = 0; g < order.length; ++g) {
            const group = order[g];
            const items = byGroup[group];
            // While filtering, a collapsed folder would hide its own hits.
            const shut = needle.length === 0
                         && collapsedGroups[group] === true;
            out.push({ header: true, name: group, tag: "", key: "",
                       sceneIndex: -1, count: items.length, collapsed: shut });
            if (!shut)
                for (let k = 0; k < items.length; ++k)
                    out.push({ header: false, name: items[k].name,
                               tag: items[k].tag, key: items[k].key,
                               sceneIndex: items[k].sceneIndex,
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
        const all = view.scenes;
        for (let i = 0; i < all.length; ++i)
            next[all[i].category] = shut;
        collapsedGroups = next;
        rebuildRows();
    }

    /** The row showing `sceneIndex`, or -1 when the filter hides it. */
    function rowForScene(sceneIndex) {
        for (let i = 0; i < rows.length; ++i)
            if (!rows[i].header && rows[i].sceneIndex === sceneIndex)
                return i;
        return -1;
    }

    /** Opens the folder holding the running scene and scrolls to it. The
     *  scene can change from outside the list (`--shot --scene`, and any
     *  future jump-to), and a selection you cannot see is not a selection. */
    function revealCurrent() {
        const scene = view.scenes[view.sceneIndex];
        if (scene === undefined)
            return;
        if (collapsedGroups[scene.category] === true)
            toggleGroup(scene.category);
        const rowIndex = rowForScene(view.sceneIndex);
        if (rowIndex >= 0)
            sceneList.positionViewAtIndex(rowIndex, ListView.Contain);
    }

    Connections {
        target: view
        function onSceneIndexChanged() { window.revealCurrent(); }
    }

    /** Arrow keys move between SCENES, stepping over folder headers — and
     *  land somewhere sensible when the current scene is filtered out. */
    function selectDelta(step) {
        const start = rowForScene(view.sceneIndex);
        let i = start >= 0 ? start : (step > 0 ? -1 : rows.length);
        for (i += step; i >= 0 && i < rows.length; i += step) {
            if (!rows[i].header) {
                view.sceneIndex = rows[i].sceneIndex;
                sceneList.positionViewAtIndex(i, ListView.Contain);
                return;
            }
        }
    }

    onFilterTextChanged: rebuildRows()

    // Open on the SHAPE of the registry, not on its first thirteen rows.
    // Forty-five scenes fully expanded is four screens of scrolling before
    // you have seen that a Kit folder exists; twelve folder headers with the
    // running scene's folder open is one screen that says what is here.
    Component.onCompleted: {
        const all = view.scenes;
        const current = all[view.sceneIndex];
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

    // Draggable split: the catalog tags are long and the metric names are
    // fixed, so how much of the window the sidebar deserves is the reader's
    // call, not a constant.
    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        handle: Rectangle {
            implicitWidth: 5
            color: SplitHandle.pressed ? "#4a3d85"
                                       : (SplitHandle.hovered ? "#2a2350"
                                                              : "#181530")
        }

        // ---- Sidebar: scene registry grouped by catalog category ----
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
                        text: "STRESS CATALOG"
                        color: "#ffb46b"
                        font.pixelSize: 15
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: view.scenes.length + " scenes"
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
                                sceneList.forceActiveFocus();
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
                    id: sceneList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // The metrics panel below is fixed-height; without a floor
                    // here a short window would collapse the list to nothing.
                    Layout.minimumHeight: 120
                    clip: true
                    focus: true
                    model: window.rows
                    // The list is folded and filtered, so a row's position
                    // says nothing about which scene it selects — the mapping
                    // runs the other way, and is -1 when the filter has hidden
                    // the running scene.
                    currentIndex: window.rowForScene(view.sceneIndex)
                    keyNavigationEnabled: false
                    Keys.onUpPressed: window.selectDelta(-1)
                    Keys.onDownPressed: window.selectDelta(1)

                    ScrollBar.vertical: ScrollBar { id: sceneScroll }

                    readonly property real rowWidth:
                        sceneList.width
                        - (sceneScroll.visible ? sceneScroll.width : 0)

                    // One delegate, two faces. A Loader per row would have to
                    // hand the row down through a required property after
                    // construction, which is exactly what required properties
                    // forbid; forty-five cheap Items cost less than that
                    // fight.
                    delegate: Item {
                        id: row

                        required property var modelData

                        width: sceneList.rowWidth
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
                                    sceneList.forceActiveFocus();
                                }
                            }
                        }

                        // ---- A scene ----
                        // Two lines, both elided: tags run to ~40 characters
                        // ("EMBER GATE — the flagship living poster"), which
                        // no single-row layout can hold beside the name.
                        Rectangle {
                            anchors.fill: parent
                            visible: !row.modelData.header
                            radius: 7
                            color: view.sceneIndex === row.modelData.sceneIndex
                                 ? "#2c2456"
                                 : sceneHover.hovered ? "#1b1730" : "transparent"

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

                            HoverHandler { id: sceneHover }
                            TapHandler {
                                onTapped: {
                                    view.sceneIndex = row.modelData.sceneIndex;
                                    sceneList.forceActiveFocus();
                                }
                            }
                            // A study says where it lives: the file you would
                            // open to change what you are looking at.
                            ToolTip.visible: sceneHover.hovered
                            ToolTip.delay: 700
                            ToolTip.text: row.modelData.key.length > 0
                                ? row.modelData.name + " — "
                                  + row.modelData.tag + "\n"
                                  + "sketches/" + row.modelData.key + ".cpp"
                                : row.modelData.name + " — "
                                  + row.modelData.tag
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

                // ---- Live frame metrics (the FPS measurement) ----
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
                                name: "recon"
                                value: (window.stats.reconcileMs ?? 0).toFixed(2)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "layout"
                                value: (window.stats.layoutMs ?? 0).toFixed(2)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "volatile"
                                value: (window.stats.volatileMs ?? 0).toFixed(2)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "paint"
                                value: (window.stats.paintMs ?? 0).toFixed(2)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "instances"
                                value: String(window.stats.instances ?? 0)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "pictures"
                                value: String(window.stats.pictures ?? 0)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "textures"
                                value: String(window.stats.textures ?? 0)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "nodes"
                                value: String(window.stats.nodes ?? 0)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "canvas"
                                value: window.stats.canvas ?? "—"
                            }
                        }
                    }
                }
            }
        }

        // ---- The compose surface ----
        ComposeGalleryView {
            id: view
            SplitView.fillWidth: true
        }
    }
}
