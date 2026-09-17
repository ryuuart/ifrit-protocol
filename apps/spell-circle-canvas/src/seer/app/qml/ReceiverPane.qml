pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Qt 1.0 as Ui
import SpellCircle.Canvas 1.0

Ui.Panel {
    id: pane
    required property var receiver
    signal settingsRequested
    padding: 12

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Ui.PanelHeading { title: "Scene receiver"; detail: "SpellCircle · vector scenes"; Layout.fillWidth: true }
            Item { Layout.fillWidth: true }
            ToolButton { text: "Settings…"; onClicked: pane.settingsRequested() }
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: source
                Layout.fillWidth: true
                text: pane.receiver.uri
                placeholderText: "udp://:27015"
                Accessible.name: "Receiver source URI"
                font.family: Ui.Theme.monospaceFontFamily
                onEditingFinished: pane.receiver.uri = text
                onAccepted: pane.receiver.start()
            }
            Button {
                text: pane.receiver.listening ? "Stop" : "Start"
                onClicked: {
                    pane.receiver.uri = source.text;
                    if (pane.receiver.listening) pane.receiver.stop();
                    else pane.receiver.start();
                }
            }
        }
        Ui.StatusIndicator {
            Layout.fillWidth: true
            text: pane.receiver.statusText
            tone: pane.receiver.listening ? "good" : "neutral"
        }
        // Kept alive while the receiver is open: publication is produced by
        // this item's render pass, independently of the inspected wire.
        PreviewPane {
            id: preview
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 200
            model: pane.receiver.model
            config: pane.receiver.config
        }
        RowLayout {
            Layout.fillWidth: true
            ToolButton { text: "Fit"; onClicked: preview.fitView() }
            ToolButton { text: "100%"; onClicked: preview.zoomToActualSize() }
            ToolButton { text: "Clear"; onClicked: pane.receiver.model.clear() }
            Item { Layout.fillWidth: true }
            Label {
                text: Math.round(preview.viewScale * 100) + "% · "
                    + pane.receiver.config.canvas.width + " × " + pane.receiver.config.canvas.height
                color: Ui.Theme.secondaryText
                font.family: Ui.Theme.monospaceFontFamily
                font.pixelSize: Ui.Theme.captionSize
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Ui.SectionHeading { text: "Activity" }
            Item { Layout.fillWidth: true }
            Label {
                text: pane.receiver.model.scenesPerSecond.toFixed(1) + " scenes/s · " + activity.count + " retained"
                color: Ui.Theme.secondaryText
                font.pixelSize: Ui.Theme.captionSize
            }
        }
        ListView {
            id: activity
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            Layout.preferredHeight: 130
            Layout.maximumHeight: 130
            clip: true
            model: pane.receiver.model
            ScrollBar.vertical: ScrollBar {}
            delegate: Column {
                required property string timestamp
                required property string source
                required property string message
                width: ListView.view.width
                spacing: 2
                Label {
                    width: parent.width
                    text: parent.message
                    elide: Text.ElideRight
                    color: Ui.Theme.primaryText
                    font.pixelSize: Ui.Theme.captionSize
                }
                Label {
                    width: parent.width
                    text: parent.timestamp + " · " + parent.source
                    elide: Text.ElideMiddle
                    color: Ui.Theme.secondaryText
                    font.pixelSize: Ui.Theme.captionSize
                    font.family: Ui.Theme.monospaceFontFamily
                }
                Item { width: 1; height: 7 }
            }
        }
    }
}
