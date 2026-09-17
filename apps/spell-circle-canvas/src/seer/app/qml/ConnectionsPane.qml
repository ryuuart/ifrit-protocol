pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Ifrit.Qt 1.0 as Ui

Ui.Panel {
    id: pane

    required property var session

    /** Opens what the field spells and clears it. */
    function openTyped() {
        const uri = uriField.text.trim();
        if (uri.length === 0)
            return;
        pane.session.open(uri);
        uriField.text = "";
    }

    radius: 12
    padding: 14


    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Ui.PanelHeading { title: "Connections"; detail: wireView.count + " open"; Layout.fillWidth: true }
        ComboBox {
            id: protocol
            Layout.fillWidth: true
            model: ["Custom address", "UDP · listen", "OSC · listen", "WebSocket · listen", "MIDI · virtual input", "Art-Net · listen"]
            Accessible.name: "Connection protocol template"
            onActivated: index => {
                const templates = ["", "udp://:27020", "osc://:27050", "ws://:27060/sky", "midi://in/virtual:Seer", "artnet://:6454"];
                uriField.text = templates[index];
                uriField.forceActiveFocus();
                uriField.selectAll();
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: uriField

                Layout.fillWidth: true
                placeholderText: "Protocol and address"
                Accessible.name: "Connection address"
                font.family: Ui.Theme.monospaceFontFamily
                onAccepted: pane.openTyped()
            }

            Button {
                text: "Connect"
                enabled: uriField.text.trim().length > 0
                onClicked: pane.openTyped()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: "Schema…"
                onClicked: schemaDialog.open()
            }

            // The root the schema declares, which is what a message is
            // read AS. A reader who picked the wrong file sees another
            // name here rather than a wire that quietly shows bytes.
            Label {
                Layout.fillWidth: true
                text: pane.session.schemaRoot.length > 0 ? pane.session.schemaRoot : "no schema"
                color: pane.session.schemaRoot.length > 0 ? Ui.Theme.primaryText : Ui.Theme.disabledText
                font.family: Ui.Theme.monospaceFontFamily
                font.pixelSize: Ui.Theme.captionSize
                elide: Text.ElideMiddle
            }
        }

        ListView {
            id: wireView

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            model: pane.session.wires
            currentIndex: pane.session.selected
            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id: wire

                required property int index
                required property string uri
                required property string address
                required property string dialect
                required property string lastFrom
                required property string error
                required property real arrivalsPerSecond
                required property var generation
                required property var dropped
                required property bool closed

                width: ListView.view.width
                highlighted: pane.session.selected === wire.index
                onClicked: pane.session.selected = wire.index

                contentItem: RowLayout {
                    spacing: 10

                    // Green while messages are arriving, quiet grey while
                    // the wire is open and nothing is coming, and the
                    // disabled grey once it is closed or was never
                    // opened — with the reason in the line below it.
                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: wire.error.length > 0 ? Ui.Theme.errorText : wire.closed ? Ui.Theme.disabledText : wire.arrivalsPerSecond > 0 ? Ui.Theme.statusText : Ui.Theme.secondaryText
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: wire.uri
                            color: Ui.Theme.primaryText
                            font.family: Ui.Theme.monospaceFontFamily
                            font.pixelSize: 12
                            elide: Text.ElideMiddle
                        }

                        // What is wrong reads as the fact it is; where
                        // the wire stands is the quieter line.
                        Label {
                            Layout.fillWidth: true
                            text: wire.error.length > 0 ? wire.error : wire.address.length > 0 ? wire.address : "not bound"
                            color: wire.error.length > 0 ? Ui.Theme.errorText : Ui.Theme.secondaryText
                            font.pixelSize: Ui.Theme.captionSize
                            elide: Text.ElideRight
                        }

                        // What the wire speaks, and who is at the other
                        // end, under where this end stands. The word is
                        // read off the scheme and is there from the
                        // start; the sender arrives with the first
                        // message and not before.
                        Label {
                            Layout.fillWidth: true
                            visible: wire.dialect.length > 0 || wire.lastFrom.length > 0
                            text: wire.dialect.length === 0 ? "from " + wire.lastFrom : wire.lastFrom.length === 0 ? wire.dialect : wire.dialect + " · from " + wire.lastFrom
                            color: Ui.Theme.disabledText
                            font.family: Ui.Theme.monospaceFontFamily
                            font.pixelSize: Ui.Theme.captionSize
                            elide: Text.ElideMiddle
                        }
                    }

                    ColumnLayout {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 2

                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: wire.arrivalsPerSecond >= 0.05 ? wire.arrivalsPerSecond.toFixed(1) + "/s" : "—"
                            color: Ui.Theme.primaryText
                            font.family: Ui.Theme.monospaceFontFamily
                            font.pixelSize: Ui.Theme.captionSize
                        }

                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: wire.dropped > 0 ? wire.generation + " · " + wire.dropped + " lost" : String(wire.generation)
                            color: Ui.Theme.secondaryText
                            font.family: Ui.Theme.monospaceFontFamily
                            font.pixelSize: Ui.Theme.captionSize
                        }
                    }

                    ToolButton {
                        text: "Close"
                        font.pixelSize: Ui.Theme.captionSize
                        onClicked: pane.session.close(wire.index)
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                text: "No connections yet"
                color: Ui.Theme.disabledText
                font.pixelSize: Ui.Theme.bodySize
                visible: wireView.count === 0
            }
        }
    }

    FileDialog {
        id: schemaDialog

        fileMode: FileDialog.OpenFile
        nameFilters: ["Binary schema (*.bfbs)", "Any file (*)"]
        acceptLabel: "Read Through"
        onAccepted: pane.session.loadSchema(schemaDialog.selectedFile)
    }
}
