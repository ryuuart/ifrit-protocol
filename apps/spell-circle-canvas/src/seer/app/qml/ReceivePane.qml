// WHAT ARRIVED: the newest message in whichever reading it has, and
// every message before it as a line each with the end it came from.
//
// The readings are tabs rather than panes side by side because they are
// one message: a reader wants the bytes, or the text, or the document,
// or the packet, or the instrument's message, or the desk's universe, or
// what a schema makes of it — never two of them at once. A reading a
// message does not have is offered disabled, which is how the pane says
// what the message is not, and the wire's own scheme is what opens the
// pane on the reading that wire carries until a schema is handed over,
// which reads before any of them.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Qt 1.0 as Ui

Ui.Panel {
    id: pane

    required property var session
    readonly property var reading: pane.session.reading

    /** The reading the chosen tab asks for, falling back to the bytes
     *  themselves, which every message has. The readings stand in the
     *  order the tabs do, which is the order the wire detail numbers
     *  them. */
    function shownReading() {
        const offered = [pane.reading.hexadecimal, pane.reading.text, pane.reading.json, pane.reading.osc, pane.reading.midi, pane.reading.dmx, pane.reading.schema];
        const chosen = offered[readings.currentIndex];
        return chosen && chosen.length > 0 ? chosen : pane.reading.hexadecimal;
    }

    /** Opens the tabs on the reading the wire makes natural, which is
     *  what a reader who has not chosen one is shown. */
    function openOnNatural() {
        readings.currentIndex = pane.reading.naturalReading;
    }

    radius: 12
    padding: 14


    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Ui.SectionHeading { text: "Receive" }

            Label {
                Layout.fillWidth: true
                text: pane.reading.present ? pane.reading.uri : "no wire is being read"
                color: pane.reading.present ? Ui.Theme.primaryText : Ui.Theme.disabledText
                font.family: Ui.Theme.monospaceFontFamily
                font.pixelSize: 12
                elide: Text.ElideMiddle
            }

            Label {
                text: pane.reading.present ? pane.reading.generation + " received · " + pane.reading.byteSize + " B" : ""
                color: Ui.Theme.secondaryText
                font.family: Ui.Theme.monospaceFontFamily
                font.pixelSize: Ui.Theme.captionSize
            }
        }

        // Seven readings of one message, so the control is as wide as
        // the seven words are: stretched across the pane it would read
        // as seven panes rather than as one choice. Each word is as
        // short as the format it names, which is what keeps the row one
        // row at the narrowest the pane goes. They stand in the order
        // the wire detail numbers them, so the reading it calls natural
        // is this index.
        Ui.SegmentedControl {
            id: readings
            property bool picked: false
            Layout.alignment: Qt.AlignLeft
            model: [
                {text: "Hex"},
                {text: "Text", enabled: pane.reading.text.length > 0},
                {text: "JSON", enabled: pane.reading.json.length > 0},
                {text: "OSC", enabled: pane.reading.osc.length > 0},
                {text: "MIDI", enabled: pane.reading.midi.length > 0},
                {text: "DMX", enabled: pane.reading.dmx.length > 0},
                {text: "Schema", enabled: pane.reading.schema.length > 0}
            ]
            onActivated: index => { currentIndex = index; picked = true; }
        }

        // Why the schema reading is not there: no schema handed over, or
        // a message this schema cannot hold. A disabled tab alone says
        // the message is not that; the sentence says which of the two
        // it is, which is the difference between opening a file and
        // looking at another wire.
        Label {
            Layout.fillWidth: true
            visible: pane.reading.schemaNote.length > 0
            text: pane.reading.schemaNote
            color: Ui.Theme.secondaryText
            font.pixelSize: Ui.Theme.captionSize
            elide: Text.ElideRight
        }

        // A wire chosen is a reading chosen: what the last wire was read
        // as says nothing about this one.
        Connections {
            target: pane.session

            function onSelectionChanged() {
                readings.picked = false;
                pane.openOnNatural();
            }
        }

        // The first message on a wire is where its reading becomes
        // knowable, so a pane opened before anything arrived follows
        // until the reader takes it somewhere.
        Connections {
            target: pane.reading

            function onChanged() {
                if (!readings.picked)
                    pane.openOnNatural();
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(pane.height * 0.4)
            clip: true

            TextArea {
                readOnly: true
                text: pane.shownReading()
                placeholderText: "Nothing has arrived yet"
                color: Ui.Theme.primaryText
                font.family: Ui.Theme.monospaceFontFamily
                font.pixelSize: Ui.Theme.captionSize
                // At a word boundary, which for the hexadecimal reading is
                // between two pairs: a byte broken across two lines is a
                // byte a reader has to reassemble. A document with no
                // spaces in it is still broken wherever it has to be.
                wrapMode: TextArea.Wrap
                background: null
            }
        }

        Ui.SectionHeading { text: "Messages" }

        ListView {
            id: logView

            /** Follows the newest message until the reader scrolls away
             *  from the end, and follows again once they come back. */
            property bool following: true

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: pane.session.messages
            ScrollBar.vertical: ScrollBar {}
            onCountChanged: if (logView.following)
                logView.positionViewAtEnd()
            onMovementEnded: logView.following = logView.atYEnd

            delegate: Item {
                id: message

                required property real at
                required property var generation
                required property string from
                required property var size
                required property string preview

                width: ListView.view.width
                height: 22

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 2
                    anchors.rightMargin: 2
                    spacing: 10

                    Label {
                        text: message.generation
                        color: Ui.Theme.disabledText
                        font.family: Ui.Theme.monospaceFontFamily
                        font.pixelSize: Ui.Theme.captionSize
                        horizontalAlignment: Text.AlignRight
                        Layout.preferredWidth: 44
                    }

                    Label {
                        text: message.at.toFixed(2) + " s"
                        color: Ui.Theme.secondaryText
                        font.family: Ui.Theme.monospaceFontFamily
                        font.pixelSize: Ui.Theme.captionSize
                        horizontalAlignment: Text.AlignRight
                        Layout.preferredWidth: 64
                    }

                    // Which end sent it. One wire carries messages from
                    // many senders, and the scheme is the wire's and
                    // already above, so only the end that differs is
                    // here.
                    Label {
                        text: message.from
                        color: Ui.Theme.disabledText
                        font.family: Ui.Theme.monospaceFontFamily
                        font.pixelSize: Ui.Theme.captionSize
                        elide: Text.ElideRight
                        Layout.preferredWidth: 124
                    }

                    Label {
                        text: message.size + " B"
                        color: Ui.Theme.secondaryText
                        font.family: Ui.Theme.monospaceFontFamily
                        font.pixelSize: Ui.Theme.captionSize
                        horizontalAlignment: Text.AlignRight
                        Layout.preferredWidth: 56
                    }

                    Label {
                        Layout.fillWidth: true
                        text: message.preview
                        color: Ui.Theme.primaryText
                        font.family: Ui.Theme.monospaceFontFamily
                        font.pixelSize: Ui.Theme.captionSize
                        elide: Text.ElideRight
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                text: "Waiting for data…"
                color: Ui.Theme.disabledText
                font.pixelSize: Ui.Theme.bodySize
                visible: logView.count === 0
            }
        }
    }
}
