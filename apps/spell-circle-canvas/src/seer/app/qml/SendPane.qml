// WHAT GOES BACK: the peer, how the editor is read, and the three ways
// a message leaves — once, on every frame, or as the echo of whatever
// arrives on the wire being read.
//
// The editor holds text either way. Read as text it is sent as its
// UTF-8 bytes; read as hexadecimal it is the bytes those digits spell,
// which is how a wire carrying no text at all is answered by hand.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Ui 1.0 as Ui

Ui.GlassPanel {
    id: pane

    required property var session
    readonly property var sending: pane.session.sending

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

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: "SEND"
                color: Ui.Theme.secondaryText
                font.pixelSize: 11
                font.letterSpacing: 1.5
                font.weight: Font.DemiBold
            }

            TextField {
                Layout.fillWidth: true
                placeholderText: "udp://127.0.0.1:27020"
                text: pane.sending.peerUri
                font.family: Ui.Theme.monospaceFontFamily
                onEditingFinished: pane.sending.peerUri = text.trim()
            }

            RadioButton {
                text: "Text"
                checked: !pane.sending.hexadecimal
                onToggled: if (checked)
                    pane.sending.hexadecimal = false
            }

            RadioButton {
                text: "Hex"
                checked: pane.sending.hexadecimal
                onToggled: if (checked)
                    pane.sending.hexadecimal = true
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            TextArea {
                id: editor

                placeholderText: pane.sending.hexadecimal ? "48 65 6c 6c 6f" : "{ \"scene\": 1 }"
                color: Ui.Theme.primaryText
                font.family: Ui.Theme.monospaceFontFamily
                font.pixelSize: 12
                wrapMode: TextArea.Wrap
                background: null
                // Read once and written from then on: a two-way binding
                // on an editor fights the reader's own typing.
                Component.onCompleted: editor.text = pane.sending.message
                onTextChanged: pane.sending.message = editor.text
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                text: "Send"
                onClicked: pane.sending.sendOnce()
            }

            CheckBox {
                text: "Every 16 ms"
                checked: pane.sending.repeating
                onToggled: pane.sending.repeating = checked
            }

            CheckBox {
                text: "Loop back"
                checked: pane.sending.loopingBack
                onToggled: pane.sending.loopingBack = checked
            }

            Label {
                Layout.fillWidth: true
                text: pane.sending.note
                color: Ui.Theme.primaryText
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Label {
                text: pane.sending.sent + " sent"
                color: Ui.Theme.secondaryText
                font.family: Ui.Theme.monospaceFontFamily
                font.pixelSize: 11
            }
        }
    }
}
