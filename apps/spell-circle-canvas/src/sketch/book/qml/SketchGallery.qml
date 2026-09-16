// Delegates reach outward — a card needs the grid's selection and the
// window's selection. Bound makes those captures explicit rather than
// resolved by scope-chain accident.
pragma ComponentBehavior: Bound

// THE OTHER WAY THROUGH THE REGISTRY: every sketch as its own still.
// A list answers "what is here"; this answers "which one was that", and
// for a registry of pictures that is the question asked most often.

import QtQuick
import Ifrit.Qt 1.0 as Ui
import QtQuick.Controls
import QtQuick.Layouts
import Sigil.Sketchbook

Item {
    id: gallery

    /** The filtered sketches in display order. */
    property var cards: []
    /** The catalog cards request their thumbnails from. */
    property var catalog: null
    /** Learned session facts overlaid by sketchIndex without resetting cards. */
    property var learnedSketches: ({})
    property int selectedIndex: -1
    property int presentedIndex: -1

    signal selectRequested(int index)
    signal activateRequested(int index)
    signal stepRequested(int delta)

    function focusRows() {
        grid.forceActiveFocus();
    }
    function positionAt(row) {
        if (row >= 0)
            grid.positionViewAtIndex(row, GridView.Contain);
    }
    function scrollPosition() {
        return grid.contentY;
    }
    function restoreScrollPosition(position) {
        grid.forceLayout();
        const first = grid.originY;
        const last = Math.max(first, first + grid.contentHeight - grid.height);
        grid.contentY = Math.max(first, Math.min(position, last));
    }
    /** How many cards fit across — what an arrow up or down moves by. */
    function columns() {
        return Math.max(1, Math.floor((grid.width - (gridScroll.visible ? gridScroll.width : 0)) / 210));
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

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
            cellWidth: Math.floor((grid.width - (gridScroll.visible ? gridScroll.width : 0)) / gallery.columns())
            // The picture owns a fixed aspect ratio. Text is budgeted below
            // it instead of being asked to fit whatever a ratio left over.
            cellHeight: Math.round((grid.cellWidth - 24) * 0.625 + 136)
            Keys.onLeftPressed: gallery.stepRequested(-1)
            Keys.onRightPressed: gallery.stepRequested(1)
            Keys.onUpPressed: gallery.stepRequested(-gallery.columns())
            Keys.onDownPressed: gallery.stepRequested(gallery.columns())
            Keys.onReturnPressed: gallery.activateRequested(gallery.selectedIndex)
            Keys.onEnterPressed: gallery.activateRequested(gallery.selectedIndex)

            ScrollBar.vertical: ScrollBar {
                id: gridScroll
            }

            delegate: Item {
                id: cell

                required property var modelData
                readonly property var sketch: gallery.learnedSketches[cell.modelData.sketchIndex] ?? cell.modelData

                width: grid.cellWidth
                height: grid.cellHeight
                Accessible.role: Accessible.ListItem
                Accessible.name: cell.sketch.name
                Accessible.description: cell.sketch.available ? cell.sketch.blurb : cell.sketch.reason
                Accessible.selectable: true
                Accessible.selected: gallery.selectedIndex === cell.sketch.sketchIndex
                Accessible.onPressAction: gallery.activateRequested(cell.sketch.sketchIndex)

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 5
                    radius: 8
                    color: gallery.selectedIndex === cell.sketch.sketchIndex ? Ui.Theme.selectionBackground : Ui.Theme.solidPanelBackground
                    border.width: 1
                    border.color: gallery.presentedIndex === cell.sketch.sketchIndex ? Ui.Theme.accent : (cardHover.hovered ? Ui.Theme.border : Ui.Theme.separator)
                    opacity: 1.0

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 7

                        PlateThumb {
                            id: still

                            Layout.fillWidth: true
                            Layout.preferredHeight: still.width * 0.625
                            plate: cell.sketch.plate
                            kind: cell.sketch.kind
                            catalog: gallery.catalog
                            sketchIndex: cell.sketch.sketchIndex
                            radius: 5
                            decodeWidth: 320
                        }
                        Label {
                            Layout.fillWidth: true
                            text: cell.sketch.name
                            color: Ui.Theme.primaryText
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignLeft
                            elide: Text.ElideRight
                        }
                        Text {
                            id: blurb

                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.ceil(blurb.font.pixelSize * blurb.lineHeight * 3) + 3
                            Layout.minimumHeight: Layout.preferredHeight
                            text: cell.sketch.available ? cell.sketch.blurb : "unavailable — " + cell.sketch.reason
                            color: cell.sketch.available ? Ui.Theme.secondaryText : Ui.Theme.warningText
                            font.pixelSize: 11
                            lineHeight: 1.2
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                            elide: Text.ElideRight
                            maximumLineCount: 3
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignTop
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 7

                            Label {
                                text: cell.sketch.kind
                                color: Ui.Theme.secondaryText
                                font.family: Ui.Theme.monospaceFontFamily
                                font.pixelSize: Ui.Theme.captionSize
                            }
                            Label {
                                text: cell.sketch.canvas.length > 0 ? cell.sketch.canvas : "—"
                                color: Ui.Theme.secondaryText
                                font.family: Ui.Theme.monospaceFontFamily
                                font.pixelSize: Ui.Theme.captionSize
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Ui.IconButton {
                                visible: gallery.selectedIndex === cell.sketch.sketchIndex
                                text: "↗"
                                tooltip: "Open " + cell.sketch.name
                                enabled: cell.sketch.available
                                onClicked: gallery.activateRequested(cell.sketch.sketchIndex)
                            }
                        }
                    }

                    HoverHandler {
                        id: cardHover
                    }
                    TapHandler {
                        onTapped: {
                            gallery.selectRequested(cell.sketch.sketchIndex);
                            grid.forceActiveFocus();
                        }
                        onDoubleTapped: gallery.activateRequested(cell.sketch.sketchIndex)
                    }
                }
            }
        }
    }
}
