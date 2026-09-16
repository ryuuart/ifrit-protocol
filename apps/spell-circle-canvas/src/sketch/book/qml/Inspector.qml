// Delegates reach outward — a lane row needs the rail's own metrics.
pragma ComponentBehavior: Bound

// THE SELECTED SKETCH, AT LENGTH. Selection is a look; only Open moves
// the canvas — so everything a reader wants before deciding to open one
// has to be here, and the two blocks a sketch writes about itself at the
// top of its own file are most of it.

import QtQuick
import Ifrit.Qt 1.0 as Ui
import QtQuick.Controls
import QtQuick.Layouts
import Sigil.Sketchbook

Rectangle {
    id: rail

    /** The selected row, or an empty one when nothing is selected. */
    property var sketch
    /** The catalog the inspector's own thumbnail is requested from. */
    property var catalog: null
    /** Whether the canvas is presenting this one — which is what decides
     *  whether the live frame numbers below belong to it. */
    property bool presented: false
    property var metrics: ({})
    property string taskLine: ""
    property bool taskRunning: false

    signal openRequested
    signal frameRequested
    signal videoRequested
    signal benchRequested
    signal revealRequested
    signal tagRequested(string path)

    color: Ui.Theme.panelBackground

    Rectangle {
        width: 1
        height: parent.height
        color: Ui.Theme.separator
    }

    component Tag: Ui.SectionHeading {}

    component Fact: Ui.FactRow {
        required property string name
        property color tone: Ui.Theme.primaryText
        label: name
        valueColor: tone
    }

    Flickable {
        id: scroll

        anchors.fill: parent
        anchors.leftMargin: 19
        anchors.rightMargin: 18
        anchors.topMargin: 16
        clip: true
        contentWidth: scroll.width
        contentHeight: body.implicitHeight
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: body

            width: scroll.width
            spacing: 12

            PlateThumb {
                id: still

                Layout.fillWidth: true
                Layout.preferredHeight: still.width * 0.625
                radius: 7
                plate: rail.sketch.plate
                kind: rail.sketch.kind
                catalog: rail.catalog
                sketchIndex: rail.sketch.sketchIndex
                decodeWidth: 720
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Tag { text: rail.sketch.folder }
                Label {
                    Layout.fillWidth: true
                    text: rail.sketch.name
                    color: Ui.Theme.primaryText
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    text: rail.sketch.blurb
                    color: Ui.Theme.secondaryText
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }

            Flow {
                Layout.fillWidth: true
                Layout.preferredHeight: implicitHeight
                spacing: 5
                Repeater {
                    model: rail.sketch.tags ?? []
                    Button {
                        required property string modelData
                        text: modelData.split("/").join(" › ")
                        font.pixelSize: Ui.Theme.captionSize
                        implicitHeight: 26
                        onClicked: rail.tagRequested(modelData)
                    }
                }
            }

            // Open is the one that moves the canvas, so it stands alone
            // on its own line; the actions under it leave the window where
            // it is and answer in the line below them.
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Button {
                    Layout.fillWidth: true
                    text: rail.presented ? "Presenting" : "Open"
                    enabled: !rail.presented && rail.sketch.available
                    onClicked: rail.openRequested()
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Button {
                        Layout.fillWidth: true
                        text: "Frame"
                        enabled: !rail.taskRunning
                        ToolTip.visible: hovered
                        ToolTip.delay: 700
                        ToolTip.text: "Render one still through --frame"
                        onClicked: rail.frameRequested()
                    }
                    Button {
                        Layout.fillWidth: true
                        text: "Bench"
                        enabled: !rail.taskRunning
                        ToolTip.visible: hovered
                        ToolTip.delay: 700
                        ToolTip.text: "Run the 60 FPS gate through --bench"
                        onClicked: rail.benchRequested()
                    }
                    Button {
                        Layout.fillWidth: true
                        text: "Video"
                        enabled: !rail.taskRunning
                            && rail.sketch.videoExportable
                        ToolTip.visible: hovered
                        ToolTip.delay: 700
                        ToolTip.text: rail.sketch.videoExportable
                            ? "Export this sketch as a vertical MP4"
                            : "Video export requires a registry sketch"
                        onClicked: rail.videoRequested()
                    }
                    Button {
                        Layout.fillWidth: true
                        text: "Reveal"
                        ToolTip.visible: hovered
                        ToolTip.delay: 700
                        ToolTip.text: "Show the file in the Finder"
                        onClicked: rail.revealRequested()
                    }
                }
            }

            // What the last Frame, Video or Bench run answered. Each answers on
            // one line by design, so one line is what is kept.
            Label {
                Layout.fillWidth: true
                visible: rail.taskLine.length > 0
                text: rail.taskLine
                color: rail.taskRunning ? Ui.Theme.warningText : Ui.Theme.statusText
                font.family: Ui.Theme.monospaceFontFamily
                font.pixelSize: Ui.Theme.captionSize
                wrapMode: Text.WrapAnywhere
            }

            // ---- What the file says about itself ----
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5
                visible: rail.sketch.subject.length > 0
                Tag { text: "What it studies" }
                Label {
                    Layout.fillWidth: true
                    text: rail.sketch.subject
                    color: Ui.Theme.primaryText
                    font.pixelSize: 12
                    lineHeight: 1.25
                    wrapMode: Text.WordWrap
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5
                visible: rail.sketch.editFirst.length > 0
                Tag { text: "Edit these first" }
                Label {
                    Layout.fillWidth: true
                    text: rail.sketch.editFirst
                    color: Ui.Theme.secondaryText
                    font.family: Ui.Theme.monospaceFontFamily
                    font.pixelSize: Ui.Theme.captionSize
                    lineHeight: 1.3
                    wrapMode: Text.WordWrap
                }
            }

            // ---- The facts ----
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Fact {
                    Layout.fillWidth: true
                    name: "kind"
                    value: rail.sketch.kind.length > 0 ? rail.sketch.kind
                                                       : "not yet compiled"
                }
                Fact {
                    Layout.fillWidth: true
                    name: "canvas"
                    // Declared inside the sketch's own setup, so it is a
                    // fact of a running session: a sketch this window
                    // has never presented has not said it yet.
                    value: rail.sketch.canvas.length > 0
                        ? rail.sketch.canvas + " · " + rail.sketch.background
                        : "declared when it runs"
                    tone: rail.sketch.canvas.length > 0 ? Ui.Theme.primaryText
                                                        : Ui.Theme.secondaryText
                }
                Fact {
                    Layout.fillWidth: true
                    name: "moment"
                    value: rail.sketch.moment > 0
                        ? rail.sketch.moment.toFixed(2) + " s"
                        : (rail.sketch.canvas.length > 0
                            ? "none declared" : "declared when it runs")
                    tone: rail.sketch.moment > 0
                        ? Ui.Theme.primaryText
                        : (rail.sketch.canvas.length > 0 ? Ui.Theme.warningText
                                                         : Ui.Theme.secondaryText)
                }
                Fact {
                    Layout.fillWidth: true
                    name: "size"
                    value: rail.sketch.lines + " lines"
                }
                Fact {
                    Layout.fillWidth: true
                    name: "file"
                    value: rail.sketch.path
                }
                Fact {
                    Layout.fillWidth: true
                    visible: !rail.sketch.available
                    name: "unavailable"
                    value: rail.sketch.reason
                    tone: Ui.Theme.warningText
                }
            }

            // ---- The frame, while this is the one being presented ----
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: frameBody.implicitHeight + 22
                visible: rail.presented
                radius: 9
                color: Ui.Theme.windowBackground
                border.width: 1
                border.color: Ui.Theme.selectionBackground

                ColumnLayout {
                    id: frameBody

                    anchors.fill: parent
                    anchors.margins: 11
                    spacing: 5

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 9
                        Label {
                            text: rail.metrics.fps !== undefined
                                ? rail.metrics.fps.toFixed(0) : "—"
                            color: Ui.Theme.accent
                            font.pixelSize: 26
                            font.bold: true
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label {
                                Layout.fillWidth: true
                                text: "fps presented"
                                color: Ui.Theme.secondaryText
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: rail.metrics.backend ?? ""
                                color: Ui.Theme.warningText
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }
                    }
                    Fact {
                        Layout.fillWidth: true
                        name: "work · p99"
                        value: (rail.metrics.workMs ?? 0).toFixed(2) + " · "
                            + (rail.metrics.p99Ms ?? 0).toFixed(2) + " ms"
                    }
                    Fact {
                        Layout.fillWidth: true
                        name: "submit"
                        value: (rail.metrics.submitMs ?? 0).toFixed(2) + " ms"
                    }
                    Fact {
                        Layout.fillWidth: true
                        name: "headroom"
                        value: (rail.metrics.headroomFps ?? 0).toFixed(0)
                            + " fps"
                    }
                    // The runtime's own lanes, named by the runtime: a
                    // drawn tree and a lit set do not spend a frame on
                    // the same things, and pretending otherwise would
                    // print zeros under a heading one of them cannot
                    // fill.
                    Repeater {
                        model: rail.metrics.lanes ?? []
                        Fact {
                            required property var modelData
                            Layout.fillWidth: true
                            name: modelData.name
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

            Item { Layout.preferredHeight: 8 }
        }
    }
}
