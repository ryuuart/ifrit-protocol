// THE ONE LINE THAT NEVER MOVES: what the canvas is presenting, how the
// live host is doing with it, the clock controls, and the two numbers a
// reader glances at rather than reads.

import QtQuick
import Ifrit.Qt 1.0 as Ui
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: strip

    /** "live", "compiling", "failed" or "waiting", and the host's own
     *  line beside it. */
    property string hostState: "waiting"
    property string status: ""
    /** THE LOADING PHASE, WHILE IT LASTS: how many stills are still to
     *  be drawn, and the last sketch that was left without one. It
     *  stands where the running sketch's line stands, because until a
     *  sketch is opened that is what the app is doing. */
    property bool filling: false
    property int fillDone: 0
    property int fillTotal: 0
    property string fillNote: ""
    property string sketch: ""
    property string path: ""
    property string hints: ""
    property bool paused: false
    property real timeScale: 1.0
    property var metrics: ({})
    /** What the last capture wrote, shown for a moment and then gone. */
    property string capture: ""
    /** The native publisher's name once it is running. */
    property string publication: ""
    property string publicationError: ""
    property Action publishAction: null

    signal pauseToggled
    signal captureRequested
    signal timeScaleRequested(real scale)

    implicitHeight: 40
    color: Ui.Theme.solidPanelBackground

    Rectangle {
        width: parent.width
        height: 1
        color: Ui.Theme.separator
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 12
        spacing: 10

        Label {
            Layout.maximumWidth: 160
            Layout.minimumWidth: 0
            text: strip.filling
                ? "thumbnails " + strip.fillDone + "/" + strip.fillTotal + " …"
                : strip.sketch
            color: Ui.Theme.primaryText
            font.pixelSize: 12
            visible: text.length > 0
            elide: Text.ElideRight
        }
        Ui.StatusIndicator {
            text: strip.capture || (strip.filling ? strip.fillNote : strip.status)
            tone: strip.capture === "capture failed" ? "error"
                : strip.capture.length > 0 ? "good"
                : strip.filling || strip.hostState === "compiling" ? "warning"
                : strip.hostState === "failed" ? "error"
                : strip.hostState === "live" ? "good" : "neutral"
            busy: strip.capture.length === 0
                && (strip.filling || strip.hostState === "compiling")
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.maximumWidth: 280
        }
        // WHERE THE THING ON SCREEN LIVES: the file you would open to
        // change what you are looking at. Elided from the left, because
        // the end of a path is the part that identifies it.
        Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            visible: strip.width >= 1100
            text: strip.path
            color: Ui.Theme.secondaryText
            font.family: Ui.Theme.monospaceFontFamily
            font.pixelSize: Ui.Theme.captionSize
            elide: Text.ElideLeft
        }
        Ui.IconButton {
            objectName: "publicationButton"
            action: strip.publishAction
            tooltip: (checked ? "Stop frame publishing" : "Publish frames to other applications")
                + " (" + (Qt.platform.os === "osx" ? "⌘P" : "Ctrl+P") + ")"
        }
        Ui.StatusIndicator {
            objectName: "publicationStatus"
            Layout.preferredWidth: 175
            Layout.maximumWidth: 220
            Layout.minimumWidth: 0
            text: strip.publicationError.length > 0 ? "Publishing unavailable"
                : !strip.publishAction || !strip.publishAction.checked ? "Publishing off"
                : strip.publication.length > 0 ? "Publishing as " + strip.publication
                : "Starting publication…"
            tone: strip.publicationError.length > 0 ? "error"
                : strip.publication.length > 0 && strip.publishAction?.checked ? "good" : "neutral"
            busy: !!strip.publishAction?.checked && strip.publication.length === 0
        }
        // The keys, beside what they act on rather than after the
        // controls: this is the line a reader is already looking at
        // when they wonder how to move.
        Label {
            Layout.maximumWidth: 210
            visible: strip.width >= 1450
            text: strip.hints
            color: Ui.Theme.secondaryText
            font.family: Ui.Theme.monospaceFontFamily
            font.pixelSize: Ui.Theme.captionSize
            elide: Text.ElideRight
        }

        // ---- The clock ----
        Ui.IconButton {
            text: strip.paused ? "▶" : "❚❚"
            tooltip: strip.paused ? "Resume scene" : "Pause scene"
            font.pixelSize: Ui.Theme.captionSize
            implicitWidth: Ui.Theme.controlHeight
            implicitHeight: Ui.Theme.controlHeight
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
            Accessible.name: "Scene speed"
            onMoved: strip.timeScaleRequested(speed.value)
        }
        Label {
            text: speed.value.toFixed(2) + "×"
            color: Ui.Theme.secondaryText
            font.family: Ui.Theme.monospaceFontFamily
            font.pixelSize: Ui.Theme.captionSize
        }
        Ui.IconButton {
            text: "Capture"
            font.pixelSize: 11
            implicitHeight: Ui.Theme.controlHeight
            ToolTip.visible: hovered
            ToolTip.delay: 700
            ToolTip.text: "Save the frame on screen beside the sketch"
            onClicked: strip.captureRequested()
        }

        Label {
            visible: strip.width >= 1200
            text: (strip.metrics.backend ?? "")
                + (strip.metrics.fps !== undefined
                    ? " · " + strip.metrics.fps.toFixed(0) + " fps presented"
                    : "")
            color: Ui.Theme.secondaryText
            font.family: Ui.Theme.monospaceFontFamily
            font.pixelSize: Ui.Theme.captionSize
        }
    }
}
