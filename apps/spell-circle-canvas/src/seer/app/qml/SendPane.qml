// WHAT GOES BACK: the peer, how the editor is read, and the three ways
// a message leaves — once, on every frame, or as the echo of whatever
// arrives on the wire being read.
//
// What the peer speaks is what a reader types. On a wire that carries
// anything the editor holds the message: read as text it is sent as its
// UTF-8 bytes, and read as hexadecimal it is the bytes those digits
// spell, which is how a wire carrying no text at all is answered by
// hand. A wire that names a format asks for the message that format is
// made of — the arguments under an address, a note played on a channel,
// a universe of dimmers — since what a reader would otherwise type out
// is a run of type tags, status bytes and padding rather than anything
// they meant.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Ui 1.0 as Ui

Ui.GlassPanel {
    id: pane

    required property var session
    readonly property var sending: pane.session.sending

    /** Whether the editor is the message itself rather than one part of
     *  a format's own form, which is what decides whether how it is read
     *  is a choice a reader has. */
    readonly property bool plainEditor: pane.sending.dialect !== "osc" && pane.sending.dialect !== "midi" && pane.sending.dialect !== "dmx"

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

            // Where on the instrument the packet goes. It stands beside
            // the peer because the two together are the destination —
            // the machine, and the thing on it — and only a peer that
            // speaks OSC has one.
            TextField {
                Layout.preferredWidth: 170
                visible: pane.sending.dialect === "osc"
                placeholderText: "/sky/wind"
                text: pane.sending.oscAddress
                font.family: Ui.Theme.monospaceFontFamily
                onEditingFinished: pane.sending.oscAddress = text.trim()
            }

            // Which universe the dimmers below are for, which is the
            // other half of where a packet of levels lands: the desk,
            // and the universe on it.
            Label {
                visible: pane.sending.dialect === "dmx"
                text: "Universe"
                color: Ui.Theme.secondaryText
                font.pixelSize: 11
            }

            SpinBox {
                Layout.preferredWidth: 120
                visible: pane.sending.dialect === "dmx"
                from: 0
                to: 32767
                editable: true
                value: pane.sending.dmxUniverse
                onValueModified: pane.sending.dmxUniverse = value
            }

            // How the editor is read, which a wire naming a format
            // decides for itself: its message is that format, and a
            // choice offered there would be a choice with no effect.
            RadioButton {
                text: "Text"
                visible: pane.plainEditor
                checked: !pane.sending.hexadecimal
                onToggled: if (checked)
                    pane.sending.hexadecimal = false
            }

            RadioButton {
                text: "Hex"
                visible: pane.plainEditor
                checked: pane.sending.hexadecimal
                onToggled: if (checked)
                    pane.sending.hexadecimal = true
            }
        }

        // THE MESSAGE AN INSTRUMENT TAKES: what kind it is, the channel
        // it is played on, and the numbers that kind carries under the
        // names that kind calls them. There is no editor on such a peer,
        // because a MIDI message is three small numbers and a status
        // byte and not a thing anybody writes out.
        RowLayout {
            Layout.fillWidth: true
            visible: pane.sending.dialect === "midi"
            spacing: 10

            ComboBox {
                Layout.preferredWidth: 150
                model: pane.sending.midiKinds
                currentIndex: pane.sending.midiKinds.indexOf(pane.sending.midiKind)
                onActivated: pane.sending.midiKind = pane.sending.midiKinds[currentIndex]
            }

            Label {
                text: "Channel"
                color: Ui.Theme.secondaryText
                font.pixelSize: 11
            }

            // The channel a desk prints, 1 to 16, and not the four bits
            // the status byte carries it in.
            SpinBox {
                Layout.preferredWidth: 100
                from: 1
                to: 16
                editable: true
                value: pane.sending.midiChannel
                onValueModified: pane.sending.midiChannel = value
            }

            Label {
                text: pane.sending.midiFirstName
                color: Ui.Theme.secondaryText
                font.pixelSize: 11
            }

            // What the wire holds for this number and nothing wider: a
            // wheel travels either side of its centre, and everything
            // else on a channel is seven bits.
            SpinBox {
                Layout.preferredWidth: 116
                from: pane.sending.midiKind === "PitchBend" ? -8192 : 0
                to: pane.sending.midiKind === "PitchBend" ? 8191 : 127
                editable: true
                value: pane.sending.midiFirst
                onValueModified: pane.sending.midiFirst = value
            }

            // A kind that carries one number says so by having no name
            // for a second.
            Label {
                visible: pane.sending.midiSecondName.length > 0
                text: pane.sending.midiSecondName
                color: Ui.Theme.secondaryText
                font.pixelSize: 11
            }

            SpinBox {
                Layout.preferredWidth: 116
                visible: pane.sending.midiSecondName.length > 0
                from: 0
                to: 127
                editable: true
                value: pane.sending.midiSecond
                onValueModified: pane.sending.midiSecond = value
            }

            Item {
                Layout.fillWidth: true
            }
        }

        // What the editor would have stood in, so the buttons keep the
        // foot of the pane on a peer whose message is a form.
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: pane.sending.dialect === "midi"
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: pane.sending.dialect !== "midi"
            clip: true

            TextArea {
                id: editor

                placeholderText: pane.sending.dialect === "osc" ? "[0.5]" : pane.sending.dialect === "dmx" ? "[255, 128, 0]" : pane.sending.hexadecimal ? "48 65 6c 6c 6f" : "{ \"scene\": 1 }"
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
