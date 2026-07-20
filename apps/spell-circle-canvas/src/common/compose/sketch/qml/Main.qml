import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Compose.Sketch

ApplicationWindow {
    id: window
    width: 960
    height: 740
    visible: true
    title: "ComposeSketch — live canvas"
    color: "#0b0a14"

    function showCapture(path) {
        captureLabel.text = path.length > 0
            ? "saved " + path.split("/").pop() : "capture failed"
        captureLabel.opacity = 1
        captureHide.restart()
    }
    Timer {
        id: captureHide
        interval: 2500
        onTriggered: captureLabel.opacity = 0
    }
    Shortcut {
        sequence: StandardKey.Save
        onActivated: window.showCapture(view.capture())
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ComposeSketchView {
            id: view
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        // Compile-error overlay: the last good sketch keeps running
        // underneath, p5 style.
        Rectangle {
            Layout.fillWidth: true
            visible: view.errorLog.length > 0
            color: "#2a0c12"
            Layout.preferredHeight: Math.min(
                errorText.implicitHeight + 20, window.height * 0.4)
            ScrollView {
                anchors.fill: parent
                anchors.margins: 10
                Text {
                    id: errorText
                    text: view.errorLog
                    color: "#ff9aa4"
                    font.family: "Menlo"
                    font.pixelSize: 12
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                    width: window.width - 20
                }
            }
        }

        // Status bar: state dot + build status | frame metrics.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: "#12101e"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                // The state dot: green live, amber compiling (pulsing),
                // red failed, gray waiting.
                Rectangle {
                    id: stateDot
                    width: 10
                    height: 10
                    radius: 5
                    color: view.state === "live" ? "#5bd47a"
                         : view.state === "compiling" ? "#ffb46b"
                         : view.state === "failed" ? "#ff5a6e"
                         : "#5a5f73"
                    SequentialAnimation on opacity {
                        running: view.state === "compiling"
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.25; duration: 350 }
                        NumberAnimation { to: 1.0; duration: 350 }
                        onRunningChanged: if (!running) stateDot.opacity = 1
                    }
                }
                Label {
                    text: view.status
                    color: view.state === "failed" ? "#ff9aa4"
                         : view.state === "compiling" ? "#ffd9a0"
                         : "#7ee8ff"
                    font.pixelSize: 13
                }
                Button {
                    text: "capture"
                    onClicked: window.showCapture(view.capture())
                }
                Label {
                    id: captureLabel
                    color: "#5bd47a"
                    font.pixelSize: 12
                    opacity: 0
                    Behavior on opacity { NumberAnimation { duration: 300 } }
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: view.metrics
                    visible: view.metrics.length > 0
                    color: "#9aa4bb"
                    font.family: "Menlo"
                    font.pixelSize: 12
                }
                Label {
                    text: "save to reload"
                    color: "#5a5f73"
                    font.pixelSize: 12
                }
            }
        }
    }
}
