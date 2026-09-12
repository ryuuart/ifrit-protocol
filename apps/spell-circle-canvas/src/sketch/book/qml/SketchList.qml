// Delegates reach outward — a row needs the list's columns and the
// window's selection. Bound makes those captures explicit rather than
// resolved by scope-chain accident.
pragma ComponentBehavior: Bound

// The selected group as rows with sortable columns.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sigil.Sketchbook

Item {
    id: list

    /** The filtered sketches in display order. */
    property var rows: []
    /** The catalog rows request their thumbnails from. */
    property var catalog: null
    /** Learned session facts overlaid by sketchIndex without resetting rows. */
    property var learnedSketches: ({})
    /** The row the inspector is showing, and the one the canvas is
     *  presenting. They are different questions: browsing moves the
     *  first without disturbing the second. */
    property int selectedIndex: -1
    property int presentedIndex: -1
    property string sortKey: "name"
    property bool sortAscending: true

    signal selectRequested(int index)
    signal activateRequested(int index)
    signal sortRequested(string key)
    signal stepRequested(int delta)

    function focusRows() { rowList.forceActiveFocus(); }
    function positionAt(row) {
        if (row >= 0)
            rowList.positionViewAtIndex(row, ListView.Contain);
    }
    function scrollPosition() { return rowList.contentY; }
    function restoreScrollPosition(position) {
        rowList.forceLayout();
        const first = rowList.originY;
        const last = Math.max(
            first, first + rowList.contentHeight - rowList.height);
        rowList.contentY = Math.max(first, Math.min(position, last));
    }

    // The columns, stated once: a heading and its row are the same
    // grid, and a width that disagreed would show as a heading over the
    // wrong column rather than as a layout error.
    readonly property int colThumb: 66
    readonly property int colFolder: 128
    readonly property int colKind: 50
    readonly property int colCanvas: 78
    readonly property int colMoment: 58
    readonly property int colLines: 48
    readonly property int gap: 10

    // THE COLUMNS GIVE WAY FROM THE RIGHT as the pane narrows, and the
    // name never does: a row with no name is unreadable.
    readonly property bool showFolder: list.width > 700
    readonly property bool showCanvas: list.width > 520
    readonly property bool showMoment: list.width > 460
    readonly property bool showLines: list.width > 410
    readonly property bool showKind: list.width > 380

    /** One column heading: says what the column is, says whether the
     *  list is ordered by it, and asks for that ordering when clicked. */
    component Heading: Item {
        id: heading

        required property string key
        required property string label
        property int align: Text.AlignLeft

        implicitHeight: 24

        Label {
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: heading.align
            text: list.sortKey === heading.key
                ? heading.label + (list.sortAscending ? " ▲" : " ▼")
                : heading.label
            color: list.sortKey === heading.key ? Theme.label : Theme.faintest
            font.family: Theme.mono
            font.pixelSize: 9
            font.letterSpacing: 0.6
            font.capitalization: Font.AllUppercase
            elide: Text.ElideRight
        }
        TapHandler { onTapped: list.sortRequested(heading.key) }
        HoverHandler { cursorShape: Qt.PointingHandCursor }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12 + (rowScroll.visible ? rowScroll.width : 0)
            Layout.topMargin: 6
            spacing: list.gap

            Item { Layout.preferredWidth: list.colThumb }
            Heading {
                Layout.fillWidth: true
                Layout.minimumWidth: 120
                key: "name"
                label: "sketch"
            }
            Heading {
                Layout.preferredWidth: list.colFolder
                visible: list.showFolder
                key: "folder"
                label: "folder"
            }
            Heading {
                Layout.preferredWidth: list.colKind
                visible: list.showKind
                key: "kind"
                label: "kind"
            }
            Heading {
                Layout.preferredWidth: list.colCanvas
                visible: list.showCanvas
                key: "canvas"
                label: "canvas"
            }
            Heading {
                Layout.preferredWidth: list.colMoment
                visible: list.showMoment
                key: "moment"
                label: "moment"
            }
            Heading {
                Layout.preferredWidth: list.colLines
                visible: list.showLines
                key: "lines"
                label: "lines"
                align: Text.AlignRight
            }
        }

        ListView {
            id: rowList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            focus: true
            model: list.rows
            currentIndex: -1
            // A row's position says nothing about which sketch it holds
            // — the list is filtered and sorted — so the arrows
            // move over the model rather than over the view.
            keyNavigationEnabled: false
            cacheBuffer: 400
            Keys.onUpPressed: list.stepRequested(-1)
            Keys.onDownPressed: list.stepRequested(1)
            Keys.onReturnPressed: list.activateRequested(list.selectedIndex)
            Keys.onEnterPressed: list.activateRequested(list.selectedIndex)

            ScrollBar.vertical: ScrollBar { id: rowScroll }

            readonly property real rowWidth:
                rowList.width - (rowScroll.visible ? rowScroll.width : 0)

            delegate: Item {
                id: row

                required property var modelData
                readonly property var recordedSketch: row.modelData
                readonly property var sketch:
                    list.learnedSketches[row.recordedSketch.sketchIndex]
                        ?? row.recordedSketch

                width: rowList.rowWidth
                height: 48

                // ---- A sketch ----
                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    radius: 6
                    color: list.selectedIndex === row.sketch.sketchIndex
                        ? Theme.hover
                        : (rowHover.hovered ? Theme.ruleSoft : "transparent")
                    // The outline says which one the CANVAS is on, which
                    // the fill cannot: browsing moves the selection over
                    // a sketch that keeps presenting behind it.
                    border.width: 1
                    border.color: list.presentedIndex === row.sketch.sketchIndex
                        ? Theme.accent : "transparent"
                    opacity: row.sketch.available ? 1.0 : 0.45

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        anchors.topMargin: 4
                        anchors.bottomMargin: 4
                        spacing: list.gap

                        PlateThumb {
                            Layout.preferredWidth: list.colThumb
                            Layout.fillHeight: true
                            plate: row.sketch.plate
                            kind: row.sketch.kind
                            catalog: list.catalog
                            sketchIndex: row.sketch.sketchIndex
                            decodeWidth: 320
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 120
                            spacing: 1
                            Label {
                                text: row.sketch.name
                                color: Theme.text
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: row.sketch.available
                                    ? row.sketch.blurb
                                    : "unavailable — " + row.sketch.reason
                                color: row.sketch.available ? Theme.muted
                                                            : Theme.warn
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                        Label {
                            Layout.preferredWidth: list.colFolder
                            visible: list.showFolder
                            text: row.sketch.folder
                            color: Theme.muted
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.preferredWidth: list.colKind
                            visible: list.showKind
                            text: row.sketch.kind
                            color: Theme.label
                            font.family: Theme.mono
                            font.pixelSize: 10
                        }
                        Label {
                            Layout.preferredWidth: list.colCanvas
                            visible: list.showCanvas
                            // A sketch declares its canvas from inside
                            // its own setup, so the size is a fact of a
                            // session and not of a file. Blank until one
                            // has run.
                            text: row.sketch.canvas.length > 0
                                ? row.sketch.canvas : "—"
                            color: row.sketch.canvas.length > 0 ? Theme.label
                                                                : Theme.faintest
                            font.family: Theme.mono
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.preferredWidth: list.colMoment
                            visible: list.showMoment
                            text: row.sketch.moment > 0
                                ? row.sketch.moment.toFixed(1) + " s"
                                : (row.sketch.canvas.length > 0 ? "none" : "—")
                            color: row.sketch.canvas.length === 0
                                ? Theme.faintest
                                : (row.sketch.moment > 0 ? Theme.label
                                                         : Theme.warn)
                            font.family: Theme.mono
                            font.pixelSize: 10
                        }
                        Label {
                            Layout.preferredWidth: list.colLines
                            visible: list.showLines
                            horizontalAlignment: Text.AlignRight
                            text: row.sketch.lines
                            color: Theme.faint
                            font.family: Theme.mono
                            font.pixelSize: 10
                        }
                    }

                    HoverHandler { id: rowHover }
                    TapHandler {
                        onTapped: {
                            list.selectRequested(row.sketch.sketchIndex);
                            rowList.forceActiveFocus();
                        }
                        onDoubleTapped: list.activateRequested(
                            row.sketch.sketchIndex)
                    }
                }
            }
        }
    }
}
