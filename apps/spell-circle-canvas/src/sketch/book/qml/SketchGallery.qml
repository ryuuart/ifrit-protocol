pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Qt 1.0 as Ui
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
        return Math.max(1, Math.floor((grid.width - (gridScroll.visible ? gridScroll.width : 0)) / 240));
    }

    FontMetrics {
        id: titleMetrics
        font.pixelSize: Ui.Theme.bodySize
    }
    FontMetrics {
        id: captionMetrics
        font.pixelSize: Ui.Theme.captionSize
    }
    readonly property int descriptionHeight: Math.ceil(captionMetrics.height * 1.2) * 2

    GridView {
        id: grid

        anchors.fill: parent
        clip: true
        focus: true
        model: gallery.cards
        currentIndex: -1
        keyNavigationEnabled: false
        cacheBuffer: 600
        cellWidth: Math.floor((grid.width - (gridScroll.visible ? gridScroll.width : 0)) / gallery.columns())
        // Card text and actions keep a fixed budget while session facts arrive.
        cellHeight: Math.ceil((grid.cellWidth - 32) * 0.5) + 32 + Math.ceil(titleMetrics.height) + gallery.descriptionHeight + Math.ceil(captionMetrics.height) + Ui.Theme.controlHeight + Ui.Theme.smallSpacing * 4
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
            readonly property bool selected: gallery.selectedIndex === cell.sketch.sketchIndex
            readonly property bool presented: gallery.presentedIndex === cell.sketch.sketchIndex

            width: grid.cellWidth
            height: grid.cellHeight
            Accessible.role: Accessible.ListItem
            Accessible.name: cell.sketch.name
            Accessible.description: (cell.presented ? "On canvas. " : "") + (cell.sketch.available ? cell.sketch.blurb : cell.sketch.reason)
            Accessible.selectable: true
            Accessible.selected: cell.selected
            Accessible.onPressAction: gallery.activateRequested(cell.sketch.sketchIndex)

            Ui.Panel {
                anchors.fill: parent
                anchors.margins: Ui.Theme.smallSpacing
                padding: 12
                backgroundColor: cell.selected ? Ui.Theme.selectionBackground : Ui.Theme.solidPanelBackground
                borderColor: cell.selected ? Ui.Theme.accent : (cardHover.hovered ? Ui.Theme.border : Ui.Theme.separator)

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Ui.Theme.smallSpacing

                    PlateThumb {
                        id: still

                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.ceil(still.width * 0.5)
                        Layout.minimumHeight: Layout.preferredHeight
                        plate: cell.sketch.plate
                        kind: cell.sketch.kind
                        catalog: gallery.catalog
                        sketchIndex: cell.sketch.sketchIndex
                        radius: Ui.Theme.cornerRadius
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.ceil(titleMetrics.height)
                        spacing: Ui.Theme.spacing
                        Label {
                            Layout.fillWidth: true
                            text: cell.sketch.name
                            color: Ui.Theme.primaryText
                            font.pixelSize: Ui.Theme.bodySize
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Label {
                            text: cell.sketch.path.endsWith(".py") ? "Python" : "C++"
                            color: Ui.Theme.secondaryText
                            font.pixelSize: Ui.Theme.captionSize
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.preferredHeight: gallery.descriptionHeight
                        Layout.minimumHeight: Layout.preferredHeight
                        Layout.maximumHeight: Layout.preferredHeight
                        text: cell.sketch.available ? cell.sketch.blurb : "Unavailable · " + cell.sketch.reason
                        color: cell.sketch.available ? Ui.Theme.secondaryText : Ui.Theme.warningText
                        font.pixelSize: Ui.Theme.captionSize
                        textFormat: Text.PlainText
                        lineHeight: 1.2
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                        elide: Text.ElideRight
                        maximumLineCount: 2
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.ceil(captionMetrics.height)
                        spacing: Ui.Theme.spacing
                        Label {
                            Layout.fillWidth: true
                            text: cell.sketch.external ? cell.sketch.entryPath : cell.sketch.folder
                            color: Ui.Theme.secondaryText
                            font.pixelSize: Ui.Theme.captionSize
                            elide: Text.ElideRight
                            ToolTip.visible: cardHover.hovered && truncated
                            ToolTip.text: text
                        }
                        Label {
                            text: cell.sketch.lines + " lines"
                            color: Ui.Theme.secondaryText
                            font.pixelSize: Ui.Theme.captionSize
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Ui.Theme.controlHeight
                        Layout.minimumHeight: Layout.preferredHeight
                        spacing: Ui.Theme.spacing

                        Label {
                            Layout.fillWidth: true
                            text: cell.presented ? "On canvas" : cell.sketch.kind === "set" ? "3D scene" : "Canvas"
                            color: cell.presented ? Ui.Theme.statusText : Ui.Theme.secondaryText
                            font.pixelSize: Ui.Theme.captionSize
                            font.weight: cell.presented ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }
                        Label {
                            text: cell.sketch.canvas
                            color: Ui.Theme.secondaryText
                            font.family: Ui.Theme.monospaceFontFamily
                            font.pixelSize: Ui.Theme.captionSize
                        }
                        Ui.IconButton {
                            text: "↗"
                            tooltip: "Present " + cell.sketch.name
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
