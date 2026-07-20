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

        // Status bar.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: "#12101e"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                Label {
                    text: view.status
                    color: view.errorLog.length > 0 ? "#ff9aa4"
                                                    : "#7ee8ff"
                    font.pixelSize: 13
                }
                Item { Layout.fillWidth: true }
                BusyIndicator {
                    running: view.compiling
                    visible: view.compiling
                    Layout.preferredHeight: 20
                    Layout.preferredWidth: 20
                }
                Label {
                    text: "save the sketch to reload"
                    color: "#9aa4bb"
                    font.pixelSize: 12
                }
            }
        }
    }
}
