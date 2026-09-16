pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Qt 1.0 as Ui
import Sigil.Sketchbook

Ui.Panel {
    id: rail

    /** Selection describes a sketch; opening it changes the canvas. */
    property var sketch
    property var catalog: null
    property bool presented: false
    property var metrics: ({})
    property string taskLine: ""
    property bool taskRunning: false
    readonly property bool hasSelection: (rail.sketch?.sketchIndex ?? -1) >= 0

    signal openRequested
    signal frameRequested
    signal videoRequested
    signal benchRequested
    signal revealRequested
    signal tagRequested(string path)

    component Divider: Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: Ui.Theme.separator
    }

    component Fact: Ui.FactRow {
        Layout.fillWidth: true
        labelWidth: 88
    }

    Label {
        anchors.centerIn: parent
        width: parent.width
        visible: !rail.hasSelection
        text: "Select a sketch to explore its details."
        color: Ui.Theme.secondaryText
        font.pixelSize: Ui.Theme.bodySize
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
    }

    ScrollView {
        id: scroll

        anchors.fill: parent
        visible: rail.hasSelection
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: scroll.availableWidth
            spacing: Ui.Theme.sectionSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Ui.Theme.spacing

                Ui.SectionHeading {
                    Layout.fillWidth: true
                    text: rail.sketch.folder
                }
                Label {
                    Layout.fillWidth: true
                    text: rail.sketch.name
                    color: Ui.Theme.primaryText
                    font.pixelSize: Ui.Theme.headingSize
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: rail.sketch.blurb
                    color: Ui.Theme.secondaryText
                    font.pixelSize: Ui.Theme.bodySize
                    lineHeight: 1.2
                    wrapMode: Text.WordWrap
                }
                Ui.StatusIndicator {
                    Layout.fillWidth: true
                    text: !rail.sketch.available ? "Unavailable" : rail.presented ? "On canvas" : "Ready to open"
                    tone: !rail.sketch.available ? "warning" : rail.presented ? "good" : "neutral"
                }
                Label {
                    Layout.fillWidth: true
                    visible: !rail.sketch.available
                    text: rail.sketch.reason
                    color: Ui.Theme.warningText
                    font.pixelSize: Ui.Theme.captionSize
                    wrapMode: Text.WordWrap
                }
            }

            PlateThumb {
                id: still

                Layout.fillWidth: true
                visible: !rail.presented
                Layout.preferredHeight: still.width * 0.5625
                radius: Ui.Theme.cornerRadius
                plate: rail.sketch.plate
                kind: rail.sketch.kind
                catalog: rail.catalog
                sketchIndex: rail.sketch.sketchIndex
                decodeWidth: 720
            }

            Button {
                Layout.fillWidth: true
                visible: !rail.presented
                text: "Open sketch"
                enabled: rail.sketch.available
                onClicked: rail.openRequested()
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: rail.sketch.subject.length > 0
                spacing: Ui.Theme.spacing

                Ui.SectionHeading {
                    text: "OVERVIEW"
                }
                Label {
                    Layout.fillWidth: true
                    text: rail.sketch.subject
                    color: Ui.Theme.primaryText
                    font.pixelSize: Ui.Theme.bodySize
                    lineHeight: 1.3
                    wrapMode: Text.WordWrap
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: (rail.sketch.tags ?? []).length > 0
                spacing: Ui.Theme.spacing

                Ui.SectionHeading {
                    text: "SUBJECTS"
                }
                Flow {
                    id: subjects
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    spacing: Ui.Theme.smallSpacing

                    Repeater {
                        model: rail.sketch.tags ?? []
                        Button {
                            required property string modelData
                            text: modelData.split("/").join(" › ")
                            width: Math.min(implicitWidth, subjects.width)
                            font.pixelSize: Ui.Theme.captionSize
                            implicitHeight: 26
                            ToolTip.visible: hovered
                            ToolTip.delay: 700
                            ToolTip.text: "Browse " + text
                            onClicked: rail.tagRequested(modelData)
                        }
                    }
                }
            }

            Divider {}

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Ui.Theme.spacing

                Ui.SectionHeading {
                    text: "SOURCE"
                }
                Fact {
                    label: "File"
                    value: rail.sketch.path
                }
                Fact {
                    label: "Runtime"
                    value: rail.sketch.kind.length > 0 ? rail.sketch.kind : "Not yet compiled"
                    monospace: false
                }
                Fact {
                    label: "Length"
                    value: rail.sketch.lines + " lines"
                }
                Button {
                    Layout.fillWidth: true
                    text: "Show source file"
                    ToolTip.visible: hovered
                    ToolTip.delay: 700
                    ToolTip.text: "Reveal the sketch in the file manager"
                    onClicked: rail.revealRequested()
                }
                Label {
                    Layout.fillWidth: true
                    visible: rail.sketch.editFirst.length > 0
                    text: "Start editing"
                    color: Ui.Theme.primaryText
                    font.pixelSize: Ui.Theme.captionSize
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.fillWidth: true
                    visible: rail.sketch.editFirst.length > 0
                    text: rail.sketch.editFirst
                    color: Ui.Theme.secondaryText
                    font.family: Ui.Theme.monospaceFontFamily
                    font.pixelSize: Ui.Theme.captionSize
                    lineHeight: 1.3
                    wrapMode: Text.WordWrap
                }
            }

            Divider {}

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Ui.Theme.spacing

                Ui.SectionHeading {
                    text: "EXPORT"
                }
                Fact {
                    label: "Canvas"
                    value: rail.sketch.canvas.length > 0 ? rail.sketch.canvas + " · " + rail.sketch.background : "Declared when it runs"
                    valueColor: rail.sketch.canvas.length > 0 ? Ui.Theme.primaryText : Ui.Theme.secondaryText
                }
                Fact {
                    label: "Capture at"
                    value: rail.sketch.moment > 0 ? rail.sketch.moment.toFixed(2) + " s" : rail.sketch.canvas.length > 0 ? "None declared" : "Declared when it runs"
                    valueColor: rail.sketch.moment > 0 ? Ui.Theme.primaryText : Ui.Theme.secondaryText
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Ui.Theme.spacing

                    Button {
                        Layout.fillWidth: true
                        text: "Save frame"
                        enabled: !rail.taskRunning
                        ToolTip.visible: hovered
                        ToolTip.delay: 700
                        ToolTip.text: "Render a still image of this sketch"
                        onClicked: rail.frameRequested()
                    }
                    Button {
                        Layout.fillWidth: true
                        text: "Export video"
                        enabled: !rail.taskRunning && rail.sketch.videoExportable
                        ToolTip.visible: hovered
                        ToolTip.delay: 700
                        ToolTip.text: rail.sketch.videoExportable ? "Export this sketch as a vertical MP4" : "Video export requires a registry sketch"
                        onClicked: rail.videoRequested()
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: rail.taskLine.length > 0
                    text: rail.taskLine
                    color: rail.taskRunning ? Ui.Theme.warningText : Ui.Theme.statusText
                    font.pixelSize: Ui.Theme.captionSize
                    wrapMode: Text.WrapAnywhere
                }
            }

            Divider {}

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Ui.Theme.spacing

                RowLayout {
                    Layout.fillWidth: true

                    Ui.SectionHeading {
                        Layout.fillWidth: true
                        text: "PERFORMANCE"
                    }
                    Label {
                        visible: rail.presented
                        text: rail.metrics.fps !== undefined ? rail.metrics.fps.toFixed(0) + " fps" : "— fps"
                        color: Ui.Theme.primaryText
                        font.pixelSize: Ui.Theme.bodySize
                        font.weight: Font.DemiBold
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: rail.presented
                    text: rail.metrics.backend ?? ""
                    color: Ui.Theme.secondaryText
                    font.pixelSize: Ui.Theme.captionSize
                    wrapMode: Text.WordWrap
                }
                Button {
                    Layout.fillWidth: true
                    text: "Run benchmark"
                    enabled: !rail.taskRunning
                    ToolTip.visible: hovered
                    ToolTip.delay: 700
                    ToolTip.text: "Measure this sketch against the 60 FPS target"
                    onClicked: rail.benchRequested()
                }
                ToolButton {
                    id: timingDetails
                    Layout.fillWidth: true
                    visible: rail.presented
                    text: checked ? "Hide frame timings" : "Show frame timings"
                    checkable: true
                    font.pixelSize: Ui.Theme.captionSize
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: rail.presented && timingDetails.checked
                    spacing: Ui.Theme.spacing

                    Fact {
                        label: "Work · p99"
                        value: (rail.metrics.workMs ?? 0).toFixed(2) + " · " + (rail.metrics.p99Ms ?? 0).toFixed(2) + " ms"
                    }
                    Fact {
                        label: "Submit"
                        value: (rail.metrics.submitMs ?? 0).toFixed(2) + " ms"
                    }
                    Fact {
                        label: "Headroom"
                        value: (rail.metrics.headroomFps ?? 0).toFixed(0) + " fps"
                    }
                    Repeater {
                        model: rail.metrics.lanes ?? []
                        Fact {
                            required property var modelData
                            label: modelData.name
                            value: modelData.ms.toFixed(2) + " ms"
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: rail.metrics.counters ?? ""
                        visible: text.length > 0
                        color: Ui.Theme.secondaryText
                        font.family: Ui.Theme.monospaceFontFamily
                        font.pixelSize: Ui.Theme.captionSize
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
