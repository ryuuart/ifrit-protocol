// THE ONE LINE THAT NEVER MOVES: what the canvas is presenting, how the
// live host is doing with it, the clock controls, and the two numbers a
// reader glances at rather than reads.

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sigil.Sketchbook

Rectangle {
    id: strip

    /** "live", "compiling", "failed" or "waiting", and the host's own
     *  line beside it. */
    property string hostState: "waiting"
    property string status: ""
    property string sketch: ""
    property string path: ""
    property string hints: ""
    property bool paused: false
    property real timeScale: 1.0
    property var metrics: ({})
    /** What the last capture wrote, shown for a moment and then gone. */
    property string capture: ""

    signal pauseToggled
    signal captureRequested
    signal timeScaleRequested(real scale)

    implicitHeight: 32
    color: Theme.panel

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.ruleSoft
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 12
        spacing: 10

        // Green live, amber compiling (pulsing), red failed, grey
        // waiting.
        Rectangle {
            id: dot

            Layout.preferredWidth: 8
            Layout.preferredHeight: 8
            radius: 4
            color: strip.hostState === "live" ? Theme.good
                 : strip.hostState === "compiling" ? Theme.warn
                 : strip.hostState === "failed" ? "#ff5a6e"
                 : "#5a5f73"
            SequentialAnimation on opacity {
                running: strip.hostState === "compiling"
                loops: Animation.Infinite
                NumberAnimation { to: 0.25; duration: 350 }
                NumberAnimation { to: 1.0; duration: 350 }
                onRunningChanged: if (!running) dot.opacity = 1
            }
        }
        Label {
            text: strip.sketch
            color: Theme.text
            font.pixelSize: 12
            visible: text.length > 0
        }
        Label {
            text: strip.status
            color: strip.hostState === "failed" ? Theme.bad
                 : strip.hostState === "compiling" ? "#ffd9a0" : Theme.good
            font.family: Theme.mono
            font.pixelSize: 11
        }
        // WHERE THE THING ON SCREEN LIVES: the file you would open to
        // change what you are looking at. Elided from the left, because
        // the end of a path is the part that identifies it.
        Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: strip.path
            color: Theme.faintest
            font.family: Theme.mono
            font.pixelSize: 10
            elide: Text.ElideLeft
        }
        Label {
            text: strip.capture
            color: Theme.good
            font.family: Theme.mono
            font.pixelSize: 10
            visible: text.length > 0
        }
        // The keys, beside what they act on rather than after the
        // controls: this is the line a reader is already looking at
        // when they wonder how to move.
        Label {
            Layout.maximumWidth: 210
            text: strip.hints
            color: Theme.faintest
            font.family: Theme.mono
            font.pixelSize: 10
            elide: Text.ElideRight
        }

        // ---- The clock ----
        ToolButton {
            text: strip.paused ? "▶" : "❚❚"
            font.pixelSize: 10
            implicitWidth: 26
            implicitHeight: 22
            ToolTip.visible: hovered
            ToolTip.delay: 700
            ToolTip.text: strip.paused ? "Resume the scene clock"
                                       : "Hold the scene clock"
            onClicked: strip.pauseToggled()
        }
        Slider {
            id: speed

            implicitWidth: 84
            from: 0.1
            to: 4.0
            value: strip.timeScale
            onMoved: strip.timeScaleRequested(speed.value)
        }
        Label {
            text: speed.value.toFixed(2) + "×"
            color: Theme.muted
            font.family: Theme.mono
            font.pixelSize: 10
        }
        ToolButton {
            text: "Capture"
            font.pixelSize: 11
            implicitHeight: 22
            ToolTip.visible: hovered
            ToolTip.delay: 700
            ToolTip.text: "Save the frame on screen beside the sketch"
            onClicked: strip.captureRequested()
        }

        Label {
            text: (strip.metrics.backend ?? "")
                + (strip.metrics.fps !== undefined
                    ? " · " + strip.metrics.fps.toFixed(0) + " fps presented"
                    : "")
            color: Theme.faint
            font.family: Theme.mono
            font.pixelSize: 10
        }
    }
}
