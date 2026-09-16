import QtQuick
import Ifrit.Qt 1.0 as Ui
import QtQuick.Controls
import QtQuick.Layouts

Ui.Panel {
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

    padding: Ui.Theme.spacing
    contentItem: RowLayout {
        spacing: Ui.Theme.spacing
        Ui.StatusIndicator {
            Layout.fillWidth: true
            Layout.minimumWidth: 60
            Layout.maximumWidth: 280
            text: strip.capture || (strip.filling ? "Preparing previews · " + strip.fillDone + " / " + strip.fillTotal : strip.hostState === "live" ? (strip.paused ? "Paused" : "Live") : strip.status || "Ready")
            tone: strip.capture === "capture failed" ? "error" : strip.capture.length > 0 ? "good" : strip.filling || strip.hostState === "compiling" ? "warning" : strip.hostState === "failed" ? "error" : strip.hostState === "live" && !strip.paused ? "good" : "neutral"
            busy: strip.capture.length === 0 && (strip.filling || strip.hostState === "compiling")
        }
        Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            visible: strip.width >= 1200
            text: strip.path
            color: Ui.Theme.secondaryText
            font.pixelSize: Ui.Theme.captionSize
            elide: Text.ElideLeft
        }
        Item {
            Layout.fillWidth: true
        }
        Ui.IconButton {
            objectName: "publicationButton"
            action: strip.publishAction
            tooltip: (checked ? "Stop frame publishing" : "Publish frames to other applications") + " (" + (Qt.platform.os === "osx" ? "⌘P" : "Ctrl+P") + ")"
        }
        Ui.StatusIndicator {
            objectName: "publicationStatus"
            visible: !!strip.publishAction?.checked || strip.publicationError.length > 0
            Layout.maximumWidth: 210
            Layout.minimumWidth: 0
            text: strip.publicationError.length > 0 ? "Publishing unavailable" : !strip.publishAction || !strip.publishAction.checked ? "Publishing off" : strip.publication.length > 0 ? "Publishing as " + strip.publication : "Starting publication…"
            tone: strip.publicationError.length > 0 ? "error" : strip.publication.length > 0 && strip.publishAction?.checked ? "good" : "neutral"
            busy: !!strip.publishAction?.checked && strip.publication.length === 0
        }
        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: 20
            Layout.leftMargin: Ui.Theme.spacing
            Layout.rightMargin: Ui.Theme.spacing
            color: Ui.Theme.separator
        }
        Ui.IconButton {
            text: strip.paused ? "▶" : "❚❚"
            tooltip: strip.paused ? "Resume scene" : "Pause scene"
            implicitWidth: Ui.Theme.controlHeight
            onClicked: strip.pauseToggled()
        }
        Slider {
            id: speed
            implicitWidth: 80
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
            tooltip: "Save the frame on screen beside the sketch"
            onClicked: strip.captureRequested()
        }
        Label {
            visible: strip.width >= 1100
            Layout.leftMargin: Ui.Theme.spacing
            text: (strip.metrics.backend ?? "") + (strip.metrics.fps !== undefined ? " · " + strip.metrics.fps.toFixed(0) + " fps" : "")
            color: Ui.Theme.secondaryText
            font.family: Ui.Theme.monospaceFontFamily
            font.pixelSize: Ui.Theme.captionSize
        }
    }
}
