// Delegates reach outward — a card needs the grid's selection and the
// window's folder. Bound makes those captures explicit rather than
// resolved by scope-chain accident.
pragma ComponentBehavior: Bound

// THE OTHER WAY THROUGH THE REGISTRY: every sketch as its own still.
// A list answers "what is here"; this answers "which one was that", and
// for a registry of pictures that is the question asked most often.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sigil.Sketchbook

Item {
    id: gallery

    /** The sketches the filter left, already sorted. No headers: a grid
     *  reads by picture, and the folders are the chips over it. */
    property var cards: []
    /** Every folder with a count, plus the one that is on. */
    property var folders: []
    property string folder: ""
    property int selectedIndex: -1
    property int presentedIndex: -1

    signal selectRequested(int index)
    signal activateRequested(int index)
    signal folderRequested(string folder)
    signal stepRequested(int delta)

    function focusRows() { grid.forceActiveFocus(); }
    function positionAt(row) {
        if (row >= 0)
            grid.positionViewAtIndex(row, GridView.Contain);
    }
    /** How many cards fit across — what an arrow up or down moves by. */
    function columns() { return Math.max(1, Math.floor(grid.width / 230)); }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- The folders, as one row of switches ----
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: "transparent"

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.ruleSoft
            }

            ListView {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.topMargin: 8
                anchors.bottomMargin: 8
                orientation: ListView.Horizontal
                spacing: 6
                clip: true
                model: gallery.folders

                delegate: Rectangle {
                    id: chip

                    required property var modelData
                    readonly property bool on:
                        gallery.folder === chip.modelData.folder

                    width: chipRow.implicitWidth + 20
                    height: 26
                    radius: 6
                    color: chip.on ? Theme.selection : Theme.panel
                    border.width: 1
                    border.color: chip.on ? Theme.border : Theme.selection

                    RowLayout {
                        id: chipRow

                        anchors.centerIn: parent
                        spacing: 6

                        Label {
                            text: chip.modelData.label
                            color: chip.on ? Theme.text : Theme.label
                            font.pixelSize: 11
                            font.letterSpacing: 0.4
                            font.capitalization: Font.AllUppercase
                        }
                        Label {
                            text: chip.modelData.count
                            color: Theme.faint
                            font.family: Theme.mono
                            font.pixelSize: 10
                        }
                    }

                    TapHandler {
                        onTapped: gallery.folderRequested(chip.modelData.folder)
                    }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
            }
        }

        GridView {
            id: grid

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 12
            clip: true
            focus: true
            model: gallery.cards
            currentIndex: -1
            keyNavigationEnabled: false
            cacheBuffer: 600
            cellWidth: Math.floor(
                (grid.width - (gridScroll.visible ? gridScroll.width : 0))
                / gallery.columns())
            cellHeight: Math.round(grid.cellWidth * 0.92)
            Keys.onLeftPressed: gallery.stepRequested(-1)
            Keys.onRightPressed: gallery.stepRequested(1)
            Keys.onUpPressed: gallery.stepRequested(-gallery.columns())
            Keys.onDownPressed: gallery.stepRequested(gallery.columns())
            Keys.onReturnPressed: gallery.activateRequested(
                gallery.selectedIndex)
            Keys.onEnterPressed: gallery.activateRequested(
                gallery.selectedIndex)

            ScrollBar.vertical: ScrollBar { id: gridScroll }

            delegate: Item {
                id: cell

                required property var modelData

                width: grid.cellWidth
                height: grid.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 5
                    radius: 8
                    color: gallery.selectedIndex === cell.modelData.sketchIndex
                        ? Theme.hover : Theme.panel
                    border.width: 1
                    border.color:
                        gallery.presentedIndex === cell.modelData.sketchIndex
                            ? Theme.accent
                            : (cardHover.hovered ? Theme.border : Theme.rule)
                    opacity: cell.modelData.available ? 1.0 : 0.45

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 7
                        spacing: 5

                        PlateThumb {
                            id: still

                            Layout.fillWidth: true
                            Layout.preferredHeight: still.width * 0.625
                            plate: cell.modelData.plate
                            kind: cell.modelData.kind
                            radius: 5
                            decodeWidth: 320
                        }
                        Label {
                            Layout.fillWidth: true
                            text: cell.modelData.name
                            color: Theme.text
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: cell.modelData.available
                                ? cell.modelData.blurb
                                : "unavailable — " + cell.modelData.reason
                            color: cell.modelData.available ? Theme.muted
                                                            : Theme.warn
                            font.pixelSize: 11
                            lineHeight: 1.2
                            wrapMode: Text.WordWrap
                            elide: Text.ElideRight
                            maximumLineCount: 2
                            verticalAlignment: Text.AlignTop
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 7

                            Label {
                                text: cell.modelData.kind
                                color: Theme.faint
                                font.family: Theme.mono
                                font.pixelSize: 10
                            }
                            Label {
                                text: cell.modelData.canvas.length > 0
                                    ? cell.modelData.canvas : "—"
                                color: Theme.faint
                                font.family: Theme.mono
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: cell.modelData.lines + " ln"
                                color: Theme.faint
                                font.family: Theme.mono
                                font.pixelSize: 10
                            }
                        }
                    }

                    HoverHandler { id: cardHover }
                    TapHandler {
                        onTapped: {
                            gallery.selectRequested(
                                cell.modelData.sketchIndex);
                            grid.forceActiveFocus();
                        }
                        onDoubleTapped: gallery.activateRequested(
                            cell.modelData.sketchIndex)
                    }
                }
            }
        }
    }
}
