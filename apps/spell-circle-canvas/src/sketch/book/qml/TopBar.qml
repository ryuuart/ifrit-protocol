// Delegates reach outward — the icons in the view toggle read which
// mode is on. Bound makes that capture explicit rather than resolved
// by scope-chain accident.
pragma ComponentBehavior: Bound

// The strip over everything: what is here, how to narrow it, and which
// of the two ways of looking at it is on.

import QtQuick
import Ifrit.Qt 1.0 as Ui
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import Sigil.Sketchbook

Rectangle {
    id: bar

    /** How many sketches there are, and how many the filter leaves. */
    property int total: 0
    property int shown: 0
    /** The selected presentation of the shared results. */
    property string viewMode: "list"
    property bool inspectorOpen: true
    property bool inspectorAvailable: true
    property bool taskRunning: false
    property bool opening: false
    property var recents: []
    property string filterText: ""

    signal filterRequested(string text)
    signal viewModeRequested(string mode)
    signal inspectorToggled
    signal videoRequested
    signal openFileRequested
    signal openWorkspaceRequested
    signal recentRequested(var recent)
    signal clearRecentsRequested
    signal recentsRequested
    /** The filter field gives up the keyboard downwards: typing narrows
     *  the list, and the arrow that follows should move in it. */
    signal steppedOut

    function focusFilter() {
        field.forceActiveFocus();
        field.selectAll();
    }

    implicitHeight: 56
    color: Ui.Theme.toolbarBackground

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Ui.Theme.separator
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 12
        spacing: 12

        Label {
            text: "Sketchbook"
            color: Ui.Theme.primaryText
            font.pixelSize: Ui.Theme.headingSize
            font.weight: Font.DemiBold
        }
        Button {
            id: openButton

            text: "Open ▾"
            implicitHeight: Ui.Theme.controlHeight
            enabled: !bar.opening
            Accessible.name: "Open sketch or workspace"
            onClicked: openMenu.popup()

            Menu {
                id: openMenu

                y: openButton.height
                onAboutToShow: bar.recentsRequested()

                MenuItem {
                    text: "Open Sketch…"
                    onTriggered: bar.openFileRequested()
                }
                MenuItem {
                    text: "Open Workspace…"
                    onTriggered: bar.openWorkspaceRequested()
                }
                Menu {
                    id: recentMenu

                    title: "Open Recent"

                    Instantiator {
                        model: bar.recents
                        delegate: MenuItem {
                            required property var modelData

                            text: modelData.name
                                + (modelData.kind === "folder" ? "/" : "")
                                + (modelData.exists ? "" : " — missing")
                            enabled: modelData.exists && !bar.opening
                            Accessible.description: modelData.path
                            onTriggered: bar.recentRequested(modelData)
                        }
                        onObjectAdded: (index, object) => recentMenu.insertItem(index, object)
                        onObjectRemoved: (index, object) => recentMenu.removeItem(object)
                    }
                    MenuItem {
                        text: "No recent locations"
                        visible: bar.recents.length === 0
                        enabled: false
                        height: visible ? implicitHeight : 0
                    }
                    MenuSeparator { visible: bar.recents.length > 0 }
                    MenuItem {
                        text: "Clear Recent Locations"
                        enabled: bar.recents.length > 0
                        onTriggered: bar.clearRecentsRequested()
                    }
                }
            }
        }
        Ui.SearchField {
            id: field
            Layout.fillWidth: true
            Layout.minimumWidth: 160
            Layout.maximumWidth: 460
            text: bar.filterText
            placeholderText: "Search sketches or tags"
            onTextEdited: bar.filterRequested(text)
            onSteppedOut: bar.steppedOut()
            onAccepted: bar.steppedOut()
            ToolTip.visible: hovered && !activeFocus
            ToolTip.delay: 900
            ToolTip.text: "Search names and descriptions, or use tag:, kind:, folder:"
        }
        Label {
            visible: bar.width >= 1150
            text: bar.shown === bar.total ? bar.total + " sketches"
                : bar.shown + " of " + bar.total
            color: Ui.Theme.secondaryText
            font.pixelSize: Ui.Theme.captionSize
        }
        Item { Layout.fillWidth: true }
        Button {
            text: "Export all…"
            enabled: !bar.taskRunning
            implicitHeight: Ui.Theme.controlHeight
            ToolTip.visible: hovered
            ToolTip.delay: 700
            ToolTip.text: "Export every available sketch as a vertical MP4"
            onClicked: bar.videoRequested()
        }
        Ui.SegmentedControl {
            model: [{text: "Gallery"}, {text: "List"}]
            currentIndex: bar.viewMode === "gallery" ? 0 : 1
            onActivated: index => bar.viewModeRequested(index === 0 ? "gallery" : "list")
        }
        Ui.IconButton {
            text: "ⓘ"
            checked: bar.inspectorOpen
            enabled: bar.inspectorAvailable
            tooltip: !bar.inspectorAvailable ? "Widen the window to show details"
                : bar.inspectorOpen ? "Hide sketch details" : "Show sketch details"
            onClicked: bar.inspectorToggled()
        }
    }
}
