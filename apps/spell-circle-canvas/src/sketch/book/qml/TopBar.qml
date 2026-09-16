pragma ComponentBehavior: Bound

import QtQuick
import Ifrit.Qt 1.0 as Ui
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

Item {
    id: bar
    property string workspaceName: ""
    property bool examples: true
    property bool inspectorOpen: false
    property bool taskRunning: false
    property bool opening: false
    property var recents: []

    signal closeWorkspaceRequested
    signal examplesRequested
    signal inspectorToggled
    signal videoRequested
    signal openFileRequested
    signal openWorkspaceRequested
    signal recentRequested(var recent)
    signal clearRecentsRequested
    signal recentsRequested
    signal searchRequested

    function focusFilter() {
        bar.searchRequested();
    }

    implicitHeight: 48
    RowLayout {
        anchors.fill: parent
        spacing: Ui.Theme.spacing
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Ui.Theme.smallSpacing
            Label {
                text: "Sketchbook"
                color: Ui.Theme.primaryText
                font.pixelSize: Ui.Theme.headingSize + 4
                font.weight: Font.DemiBold
            }
            Label {
                Layout.fillWidth: true
                text: bar.workspaceName || "Sketches, studies, and live experiments"
                color: Ui.Theme.secondaryText
                font.pixelSize: Ui.Theme.captionSize
                elide: Text.ElideRight
            }
        }
        Button {
            id: openButton

            text: "Open…"
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
                MenuItem {
                    text: "Browse Examples"
                    onTriggered: bar.examplesRequested()
                }
                MenuItem {
                    text: "Close Workspace"
                    enabled: !bar.opening && !bar.taskRunning
                    onTriggered: bar.closeWorkspaceRequested()
                }
                MenuSeparator {}
                Menu {
                    id: recentMenu

                    title: "Open Recent"

                    Instantiator {
                        model: bar.recents
                        delegate: MenuItem {
                            required property var modelData

                            text: modelData.name + (modelData.kind === "folder" ? "/" : "") + (modelData.exists ? "" : " — missing")
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
                    MenuSeparator {
                        visible: bar.recents.length > 0
                    }
                    MenuItem {
                        text: "Clear Recent Locations"
                        enabled: bar.recents.length > 0
                        onTriggered: bar.clearRecentsRequested()
                    }
                }
            }
        }
        Ui.IconButton {
            visible: bar.examples
            text: "Export all…"
            enabled: !bar.taskRunning
            tooltip: "Export every available sketch as a vertical MP4"
            onClicked: bar.videoRequested()
        }
        Ui.IconButton {
            text: "Details"
            checked: bar.inspectorOpen
            tooltip: bar.inspectorOpen ? "Close sketch details" : "Inspect selected sketch"
            onClicked: bar.inspectorToggled()
        }
    }
}
