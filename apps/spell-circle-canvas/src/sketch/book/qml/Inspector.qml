// Delegates reach outward — a lane row needs the rail's own metrics.
pragma ComponentBehavior: Bound

// THE SELECTED SKETCH, AT LENGTH. Selection is a look; only Open moves
// the canvas — so everything a reader wants before deciding to open one
// has to be here, and the two blocks a sketch writes about itself at the
// top of its own file are most of it.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sigil.Sketchbook

Rectangle {
    id: rail

    /** The selected row, or an empty one when nothing is selected. */
    property var sketch
    /** Whether the canvas is presenting this one — which is what decides
     *  whether the live frame numbers below belong to it. */
    property bool presented: false
    property var metrics: ({})
    property string taskLine: ""
    property bool taskRunning: false

    signal openRequested
    signal frameRequested
    signal benchRequested
    signal revealRequested

    color: Theme.rail

    Rectangle {
        width: 1
        height: parent.height
        color: Theme.rule
    }

    /** A heading over a block: small, quiet, and the same everywhere. */
    component Tag: Label {
        color: Theme.label
        font.pixelSize: 9
        font.letterSpacing: 0.6
        font.capitalization: Font.AllUppercase
    }

    /** One "name   value" fact. The name gives up width by eliding
     *  rather than by painting past the rail. */
    component Fact: RowLayout {
        id: fact

        required property string name
        required property string value
        property color tone: Theme.value

        spacing: 8
        Label {
            Layout.preferredWidth: 78
            text: fact.name
            color: Theme.faint
            font.pixelSize: 11
            elide: Text.ElideRight
        }
        Label {
            Layout.fillWidth: true
            text: fact.value
            color: fact.tone
            font.family: Theme.mono
            font.pixelSize: 10
            elide: Text.ElideMiddle
        }
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
                decodeWidth: 720
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Tag { text: rail.sketch.folder }
                Label {
                    Layout.fillWidth: true
                    text: rail.sketch.name
                    color: Theme.text
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    text: rail.sketch.blurb
                    color: Theme.muted
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }

            // Open is the one that moves the canvas, so it stands alone
            // on its own line; the three under it leave the window where
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
                        text: "Reveal"
                        ToolTip.visible: hovered
                        ToolTip.delay: 700
                        ToolTip.text: "Show the file in the Finder"
                        onClicked: rail.revealRequested()
                    }
                }
            }

            // What the last Frame or Bench run answered. Both answer on
            // one line by design, so one line is what is kept.
            Label {
                Layout.fillWidth: true
                visible: rail.taskLine.length > 0
                text: rail.taskLine
                color: rail.taskRunning ? Theme.warn : Theme.good
                font.family: Theme.mono
                font.pixelSize: 10
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
                    color: Theme.value
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
                    color: Theme.label
                    font.family: Theme.mono
                    font.pixelSize: 10
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
                    tone: rail.sketch.canvas.length > 0 ? Theme.value
                                                        : Theme.faintest
                }
                Fact {
                    Layout.fillWidth: true
                    name: "moment"
                    value: rail.sketch.moment > 0
                        ? rail.sketch.moment.toFixed(2) + " s"
                        : (rail.sketch.canvas.length > 0
                            ? "none declared" : "declared when it runs")
                    tone: rail.sketch.moment > 0
                        ? Theme.value
                        : (rail.sketch.canvas.length > 0 ? Theme.warn
                                                         : Theme.faintest)
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
                    tone: Theme.warn
                }
            }

            // ---- The frame, while this is the one being presented ----
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: frameBody.implicitHeight + 22
                visible: rail.presented
                radius: 9
                color: Theme.ground
                border.width: 1
                border.color: Theme.selection

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
                            color: "#7ee8ff"
                            font.pixelSize: 26
                            font.bold: true
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label {
                                Layout.fillWidth: true
                                text: "fps presented"
                                color: Theme.muted
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: rail.metrics.backend ?? ""
                                color: Theme.warn
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
                        color: Theme.muted
                        font.family: Theme.mono
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }
}
