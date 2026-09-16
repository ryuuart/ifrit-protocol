// Delegates reach outward — a row needs the list's columns and the
// window's selection. Bound makes those captures explicit rather than
// resolved by scope-chain accident.
pragma ComponentBehavior: Bound

// The selected group as rows with sortable columns.

import QtQuick
import Ifrit.Qt 1.0 as Ui
import QtQuick.Controls
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

    function focusRows() {
        rowList.forceActiveFocus();
    }
    function positionAt(row) {
        if (row >= 0)
            rowList.positionViewAtIndex(row, ListView.Contain);
    }
    function scrollPosition() {
        return rowList.contentY;
    }
    function restoreScrollPosition(position) {
        rowList.forceLayout();
        const first = rowList.originY;
        const last = Math.max(first, first + rowList.contentHeight - rowList.height);
        rowList.contentY = Math.max(first, Math.min(position, last));
    }

    // The columns, stated once: a heading and its row are the same
    // grid, and a width that disagreed would show as a heading over the
    // wrong column rather than as a layout error.
    readonly property int colThumb: 80
    readonly property int colFolder: 128
    readonly property int colKind: 50
    readonly property int colCanvas: 78
    readonly property int colMoment: 58
    readonly property int colLines: 48
    readonly property int gap: 12

    // THE COLUMNS GIVE WAY FROM THE RIGHT as the pane narrows, and the
    // name never does: a row with no name is unreadable.
    readonly property bool showFolder: list.width > 940
    readonly property bool showCanvas: list.width > 700
    readonly property bool showMoment: list.width > 800
    readonly property bool showLines: list.width > 880
    readonly property bool showKind: list.width > 600

    /** One column heading: says what the column is, says whether the
     *  list is ordered by it, and asks for that ordering when clicked. */
    component Heading: ToolButton {
        id: heading

        required property string key
        required property string label
        property int align: Text.AlignLeft

        implicitHeight: Ui.Theme.controlHeight
        padding: 0
        Accessible.name: "Sort by " + label
        onClicked: list.sortRequested(heading.key)
        background: Rectangle {
            color: heading.hovered ? Ui.Theme.controlHoverBackground : "transparent"
            radius: Ui.Theme.cornerRadius
            border.color: heading.visualFocus ? Ui.Theme.accent : "transparent"
        }

        contentItem: Label {
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: heading.align
            text: list.sortKey === heading.key ? heading.label + (list.sortAscending ? " ▲" : " ▼") : heading.label
            color: list.sortKey === heading.key ? Ui.Theme.primaryText : Ui.Theme.secondaryText
            font.weight: Font.DemiBold
            font.pixelSize: Ui.Theme.captionSize
            font.letterSpacing: 0.4
            elide: Text.ElideRight
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16 + (rowScroll.visible ? rowScroll.width : 0)
            Layout.topMargin: 0
            spacing: list.gap

            Item {
                Layout.preferredWidth: list.colThumb
            }
            Heading {
                Layout.fillWidth: true
                Layout.minimumWidth: 120
                key: "name"
                label: "Sketch"
            }
            Heading {
                Layout.preferredWidth: list.colFolder
                visible: list.showFolder
                key: "folder"
                label: "Collection"
            }
            Heading {
                Layout.preferredWidth: list.colKind
                visible: list.showKind
                key: "kind"
                label: "Kind"
            }
            Heading {
                Layout.preferredWidth: list.colCanvas
                visible: list.showCanvas
                key: "canvas"
                label: "Canvas"
            }
            Heading {
                Layout.preferredWidth: list.colMoment
                visible: list.showMoment
                key: "moment"
                label: "Moment"
            }
            Heading {
                Layout.preferredWidth: list.colLines
                visible: list.showLines
                key: "lines"
                label: "Lines"
                align: Text.AlignRight
            }
            Item {
                Layout.preferredWidth: Ui.Theme.controlHeight
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

            ScrollBar.vertical: ScrollBar {
                id: rowScroll
            }

            readonly property real rowWidth: rowList.width - (rowScroll.visible ? rowScroll.width : 0)

            delegate: Item {
                id: row

                required property var modelData
                readonly property var recordedSketch: row.modelData
                readonly property var sketch: list.learnedSketches[row.recordedSketch.sketchIndex] ?? row.recordedSketch

                readonly property bool selected: list.selectedIndex === row.sketch.sketchIndex
                readonly property bool presented: list.presentedIndex === row.sketch.sketchIndex

                width: rowList.rowWidth
                height: 92
                Accessible.role: Accessible.ListItem
                Accessible.name: row.sketch.name
                Accessible.description: (row.presented ? "On canvas. " : "") + (row.sketch.available ? row.sketch.blurb : row.sketch.reason)
                Accessible.selectable: true
                Accessible.selected: row.selected
                Accessible.onPressAction: list.activateRequested(row.sketch.sketchIndex)

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: Ui.Theme.smallSpacing
                    radius: Ui.Theme.cornerRadius
                    color: row.selected ? Ui.Theme.selectionBackground : (rowHover.hovered ? Ui.Theme.controlBackground : "transparent")
                    border.width: 1
                    border.color: row.selected ? Ui.Theme.accent : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        anchors.topMargin: 8
                        anchors.bottomMargin: 8
                        spacing: list.gap

                        PlateThumb {
                            Layout.preferredWidth: list.colThumb
                            Layout.preferredHeight: 58
                            plate: row.sketch.plate
                            kind: row.sketch.kind
                            catalog: list.catalog
                            sketchIndex: row.sketch.sketchIndex
                            decodeWidth: 320
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 120
                            spacing: Ui.Theme.smallSpacing
                            Label {
                                text: row.sketch.name
                                color: Ui.Theme.primaryText
                                font.pixelSize: Ui.Theme.bodySize
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                Layout.fillWidth: true
                                text: row.sketch.available ? row.sketch.blurb : "Unavailable · " + row.sketch.reason
                                color: row.sketch.available ? Ui.Theme.secondaryText : Ui.Theme.warningText
                                font.pixelSize: Ui.Theme.captionSize
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: row.presented ? "On canvas" : row.sketch.kind === "set" ? "3D scene" : "Canvas"
                                color: row.presented ? Ui.Theme.statusText : Ui.Theme.secondaryText
                                font.pixelSize: Ui.Theme.captionSize
                                font.weight: row.presented ? Font.DemiBold : Font.Normal
                                elide: Text.ElideRight
                            }
                        }
                        Label {
                            Layout.preferredWidth: list.colFolder
                            visible: list.showFolder
                            text: row.sketch.folder
                            color: Ui.Theme.secondaryText
                            font.pixelSize: Ui.Theme.captionSize
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.preferredWidth: list.colKind
                            visible: list.showKind
                            text: row.sketch.kind
                            color: Ui.Theme.secondaryText
                            font.family: Ui.Theme.monospaceFontFamily
                            font.pixelSize: Ui.Theme.captionSize
                        }
                        Label {
                            Layout.preferredWidth: list.colCanvas
                            visible: list.showCanvas
                            // A sketch declares its canvas from inside
                            // its own setup, so the size is a fact of a
                            // session and not of a file. Blank until one
                            // has run.
                            text: row.sketch.canvas.length > 0 ? row.sketch.canvas : "—"
                            color: Ui.Theme.secondaryText
                            font.family: Ui.Theme.monospaceFontFamily
                            font.pixelSize: Ui.Theme.captionSize
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.preferredWidth: list.colMoment
                            visible: list.showMoment
                            text: row.sketch.moment > 0 ? row.sketch.moment.toFixed(1) + " s" : (row.sketch.canvas.length > 0 ? "none" : "—")
                            color: row.sketch.canvas.length === 0 ? Ui.Theme.secondaryText : (row.sketch.moment > 0 ? Ui.Theme.secondaryText : Ui.Theme.warningText)
                            font.family: Ui.Theme.monospaceFontFamily
                            font.pixelSize: Ui.Theme.captionSize
                        }
                        Label {
                            Layout.preferredWidth: list.colLines
                            visible: list.showLines
                            horizontalAlignment: Text.AlignRight
                            text: row.sketch.lines
                            color: Ui.Theme.secondaryText
                            font.family: Ui.Theme.monospaceFontFamily
                            font.pixelSize: Ui.Theme.captionSize
                        }
                        Item {
                            Layout.preferredWidth: Ui.Theme.controlHeight
                            Layout.preferredHeight: Ui.Theme.controlHeight

                            Ui.IconButton {
                                anchors.fill: parent
                                visible: row.selected || rowHover.hovered
                                text: "↗"
                                tooltip: "Present " + row.sketch.name
                                enabled: row.sketch.available
                                onClicked: list.activateRequested(row.sketch.sketchIndex)
                            }
                        }
                    }

                    HoverHandler {
                        id: rowHover
                    }
                    TapHandler {
                        onTapped: {
                            list.selectRequested(row.sketch.sketchIndex);
                            rowList.forceActiveFocus();
                        }
                        onDoubleTapped: list.activateRequested(row.sketch.sketchIndex)
                    }
                }
            }
        }
    }
}
