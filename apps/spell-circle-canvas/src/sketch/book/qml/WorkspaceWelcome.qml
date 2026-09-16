pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Qt 1.0 as Ui

ScrollView {
    id: welcome

    property string workspacePath: ""
    property int sketchCount: 0
    property var selectedSketch: ({})
    readonly property bool hasSelection: (selectedSketch.sketchIndex ?? -1) >= 0
    signal openRequested(int index)
    signal openFileRequested
    signal revealRequested

    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    ColumnLayout {
        width: welcome.availableWidth
        spacing: Ui.Theme.sectionSpacing

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: Ui.Theme.sectionSpacing
            spacing: Ui.Theme.sectionSpacing

            Label {
                objectName: "workspaceHeading"
                Layout.fillWidth: true
                text: welcome.sketchCount > 0 ? "Choose a sketch" : "No sketches found"
                font.pixelSize: 24
                font.weight: Font.DemiBold
                color: Ui.Theme.primaryText
                wrapMode: Text.WordWrap
                Accessible.role: Accessible.Heading
            }
            Label {
                Layout.fillWidth: true
                text: welcome.workspacePath
                textFormat: Text.PlainText
                font.pixelSize: Ui.Theme.captionSize
                color: Ui.Theme.secondaryText
                wrapMode: Text.WrapAnywhere
            }
            Label {
                Layout.fillWidth: true
                text: welcome.sketchCount > 0
                    ? welcome.sketchCount + " sketches in this workspace. Select an entry in the Library, then open it. Sketchbook remembers your choice."
                    : "Add a Python file with a @sketch class or a C++ sketch, then reopen this workspace. Nested Python projects open as their own workspaces."
                font.pixelSize: Ui.Theme.bodySize
                color: Ui.Theme.secondaryText
                wrapMode: Text.WordWrap
            }

            Ui.Panel {
                Layout.fillWidth: true
                visible: welcome.hasSelection
                contentItem: ColumnLayout {
                    spacing: Ui.Theme.spacing
                    Ui.SectionHeading {
                        text: "SELECTED ENTRY"
                    }
                    Label {
                        objectName: "selectedEntryPath"
                        Layout.fillWidth: true
                        text: welcome.selectedSketch.entryPath ?? welcome.selectedSketch.path ?? ""
                        textFormat: Text.PlainText
                        font.pixelSize: Ui.Theme.bodySize
                        font.weight: Font.DemiBold
                        color: Ui.Theme.primaryText
                        wrapMode: Text.WrapAnywhere
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "This is the file Sketchbook will load."
                        font.pixelSize: Ui.Theme.captionSize
                        color: Ui.Theme.secondaryText
                    }
                    RowLayout {
                        Button {
                            objectName: "openWorkspaceEntry"
                            text: "Open selected sketch"
                            enabled: welcome.hasSelection && (welcome.selectedSketch.available ?? false)
                            onClicked: welcome.openRequested(welcome.selectedSketch.sketchIndex)
                        }
                        Ui.IconButton {
                            text: "Show file"
                            tooltip: "Reveal the selected entry file"
                            onClicked: welcome.revealRequested()
                        }
                    }
                }
            }

            Ui.SectionHeading {
                text: "WHAT RUNS"
            }
            Repeater {
                model: [
                    { title: "Sketch entry", detail: "A Python module declaring a @sketch class, or a C++ sketch. Each entry appears separately in the Library." },
                    { title: "Modules and packages", detail: "Other .py files provide shared code. __init__.py initializes an imported package; its name does not make it an entry." },
                    { title: "Python project", detail: "pyproject.toml sets dependencies and the Python environment. It does not choose a sketch or run a console script." },
                    { title: "Assets", detail: "Images, fonts and data are loaded by your sketch. They are not entry files." }
                ]
                ColumnLayout {
                    id: roleDescription
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Ui.Theme.smallSpacing
                    Label {
                        text: roleDescription.modelData.title
                        font.pixelSize: Ui.Theme.bodySize
                        font.weight: Font.DemiBold
                        color: Ui.Theme.primaryText
                    }
                    Label {
                        Layout.fillWidth: true
                        text: roleDescription.modelData.detail
                        font.pixelSize: Ui.Theme.bodySize
                        color: Ui.Theme.secondaryText
                        wrapMode: Text.WordWrap
                    }
                }
            }
            Button {
                text: "Open a sketch file…"
                onClicked: welcome.openFileRequested()
            }
        }
    }
}
