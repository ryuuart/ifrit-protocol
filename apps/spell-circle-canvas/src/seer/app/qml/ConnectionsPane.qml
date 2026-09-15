// THE WIRES: a field to open one, and the list of the ones that are
// open.
//
// A row is a whole wire in three readings — whether anything is coming,
// the URI it was opened on, and either the end the transport bound or
// the sentence saying why it bound none. A wire that could not be opened
// is listed like any other, because the reader has to see the URI they
// mistyped beside what is wrong with it.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Ui 1.0 as Ui

Ui.GlassPanel {
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

    // The ground the pane stands on, rounded to the panel's own corners
    // and outlined, so a pane reads as a pane on a window wearing the
    // machine's glass and on one painting its own opaque colour alike.
    Rectangle {
        anchors.fill: parent
        color: Ui.Theme.panelBackground
        radius: pane.radius
        border.width: 1
        border.color: Ui.Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        Label {
            text: "CONNECTIONS"
            color: Ui.Theme.secondaryText
            font.pixelSize: 11
            font.letterSpacing: 1.5
            font.weight: Font.DemiBold
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: uriField

                Layout.fillWidth: true
                placeholderText: "udp://:27020"
                font.family: Ui.Theme.monospaceFontFamily
                onAccepted: pane.openTyped()
            }

            Button {
                text: "Open"
                enabled: uriField.text.trim().length > 0
                onClicked: pane.openTyped()
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
                        width: 8
                        height: 8
                        radius: 4
                        color: wire.error.length > 0 || wire.closed ? Ui.Theme.disabledText : wire.arrivalsPerSecond > 0 ? Ui.Theme.statusText : Ui.Theme.secondaryText
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
                            color: wire.error.length > 0 ? Ui.Theme.primaryText : Ui.Theme.secondaryText
                            font.pixelSize: 11
                            elide: Text.ElideRight
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
                            font.pixelSize: 11
                        }

                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: wire.dropped > 0 ? wire.generation + " · " + wire.dropped + " lost" : String(wire.generation)
                            color: Ui.Theme.secondaryText
                            font.family: Ui.Theme.monospaceFontFamily
                            font.pixelSize: 11
                        }
                    }

                    ToolButton {
                        text: "Close"
                        font.pixelSize: 11
                        onClicked: pane.session.close(wire.index)
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                text: "No wires open"
                color: Ui.Theme.disabledText
                font.pixelSize: 13
                visible: wireView.count === 0
            }
        }
    }
}
