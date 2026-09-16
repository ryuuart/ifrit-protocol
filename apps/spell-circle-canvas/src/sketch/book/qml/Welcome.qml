pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Qt 1.0 as Ui

Item {
    id: welcome
    property var recents: []
    property bool opening: false
    property string status: ""
    signal openFileRequested()
    signal openWorkspaceRequested()
    signal examplesRequested()
    signal recentRequested(var recent)
    signal clearRecentsRequested()

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(860, parent.width - 96)
        height: Math.min(640, parent.height - 80)
        spacing: Ui.Theme.sectionSpacing

        Label {
            text: "Sketchbook"
            font.pixelSize: 36
            font.weight: Font.DemiBold
            color: Ui.Theme.primaryText
        }
        Label {
            Layout.fillWidth: true
            text: "A place for sketches, studies, and live experiments."
            font.pixelSize: Ui.Theme.headingSize
            color: Ui.Theme.secondaryText
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Ui.Theme.spacing
            Button {
                text: "Open Workspace…"
                enabled: !welcome.opening
                onClicked: welcome.openWorkspaceRequested()
            }
            Button {
                text: "Open Sketch…"
                enabled: !welcome.opening
                onClicked: welcome.openFileRequested()
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "Browse Examples"
                enabled: !welcome.opening
                onClicked: welcome.examplesRequested()
            }
        }
        Ui.Notice {
            Layout.fillWidth: true
            visible: welcome.status.length > 0
            text: welcome.status
            busy: welcome.opening
        }
        Ui.Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentItem: ColumnLayout {
                spacing: Ui.Theme.spacing
                Ui.PanelHeading {
                    Layout.fillWidth: true
                    title: "Recent locations"
                    detail: "Folders and individual sketches"
                    Ui.IconButton {
                        text: "Clear"
                        visible: welcome.recents.length > 0
                        tooltip: "Clear recent locations"
                        onClicked: welcome.clearRecentsRequested()
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: welcome.recents.length === 0
                    text: "Open a folder of sketches, choose a C++ or Python file, or explore the bundled examples. Your recent locations will appear here."
                    wrapMode: Text.WordWrap
                    color: Ui.Theme.secondaryText
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: welcome.recents
                    spacing: 2
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        id: recentLocation
                        required property var modelData
                        width: ListView.view.width
                        height: 64
                        enabled: recentLocation.modelData.exists && !welcome.opening
                        Accessible.name: recentLocation.modelData.name
                        Accessible.description: recentLocation.modelData.path
                        onClicked: welcome.recentRequested(recentLocation.modelData)
                        contentItem: ColumnLayout {
                            spacing: Ui.Theme.smallSpacing
                            Label {
                                Layout.fillWidth: true
                                text: recentLocation.modelData.name + (recentLocation.modelData.exists ? "" : " — unavailable")
                                color: Ui.Theme.primaryText
                                font.pixelSize: Ui.Theme.bodySize
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: (recentLocation.modelData.kind === "folder" ? "Workspace · " : "Sketch · ") + recentLocation.modelData.path
                                color: Ui.Theme.secondaryText
                                font.pixelSize: Ui.Theme.captionSize
                                elide: Text.ElideMiddle
                            }
                        }
                    }
                }
            }
        }
        Label {
            text: "C++ and Python · Save your source to reload the canvas"
            color: Ui.Theme.secondaryText
            font.pixelSize: Ui.Theme.captionSize
        }
    }
}
