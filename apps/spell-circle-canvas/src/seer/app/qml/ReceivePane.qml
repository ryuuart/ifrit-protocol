// WHAT ARRIVED: the newest message in whichever reading it has, and
// every message before it as a line each with the end it came from.
//
// The readings are tabs rather than panes side by side because they are
// one message: a reader wants the bytes, or the text, or the document,
// or the packet — never two of them at once. A reading a message does
// not have is offered disabled, which is how the pane says what the
// message is not, and the wire's own scheme is what opens the pane on
// the reading that wire carries.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Ui 1.0 as Ui

Ui.GlassPanel {
    id: pane

    required property var session
    readonly property var reading: pane.session.reading

    /** The reading the chosen tab asks for, falling back to the bytes
     *  themselves, which every message has. */
    function shownReading() {
        if (readings.currentIndex === 3 && pane.reading.osc.length > 0)
            return pane.reading.osc;
        if (readings.currentIndex === 2 && pane.reading.json.length > 0)
            return pane.reading.json;
        if (readings.currentIndex === 1 && pane.reading.text.length > 0)
            return pane.reading.text;
        return pane.reading.hexadecimal;
    }

    /** Opens the tabs on the reading the wire makes natural, which is
     *  what a reader who has not chosen one is shown. */
    function openOnNatural() {
        readings.currentIndex = pane.reading.naturalReading;
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

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: "RECEIVE"
                color: Ui.Theme.secondaryText
                font.pixelSize: 11
                font.letterSpacing: 1.5
                font.weight: Font.DemiBold
            }

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
                font.pixelSize: 11
            }
        }

        // Four readings of one message, so the control is as wide as the
        // four words are: stretched across the pane it would read as
        // four panes rather than as one choice. They stand in the order
        // the wire detail numbers them, so the reading it calls natural
        // is this index.
        TabBar {
            id: readings

            /** Whether the reader picked this tab themselves. Until they
             *  do, the wire's own scheme chooses; once they have, their
             *  choice stands until another wire is chosen, so a message
             *  arriving a second does not take the pane off what they
             *  are reading. */
            property bool picked: false

            Layout.alignment: Qt.AlignLeft

            TabButton {
                text: "Hex"
                width: implicitWidth
                onClicked: readings.picked = true
            }

            TabButton {
                text: "Text"
                width: implicitWidth
                enabled: pane.reading.text.length > 0
                onClicked: readings.picked = true
            }

            TabButton {
                text: "JSON"
                width: implicitWidth
                enabled: pane.reading.json.length > 0
                onClicked: readings.picked = true
            }

            TabButton {
                text: "OSC"
                width: implicitWidth
                enabled: pane.reading.osc.length > 0
                onClicked: readings.picked = true
            }
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
                font.pixelSize: 11
                // At a word boundary, which for the hexadecimal reading is
                // between two pairs: a byte broken across two lines is a
                // byte a reader has to reassemble. A document with no
                // spaces in it is still broken wherever it has to be.
                wrapMode: TextArea.Wrap
                background: null
            }
        }

        Label {
            text: "MESSAGES"
            color: Ui.Theme.secondaryText
            font.pixelSize: 11
            font.letterSpacing: 1.5
            font.weight: Font.DemiBold
        }

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
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                        Layout.preferredWidth: 44
                    }

                    Label {
                        text: message.at.toFixed(2) + " s"
                        color: Ui.Theme.secondaryText
                        font.family: Ui.Theme.monospaceFontFamily
                        font.pixelSize: 11
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
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.preferredWidth: 124
                    }

                    Label {
                        text: message.size + " B"
                        color: Ui.Theme.secondaryText
                        font.family: Ui.Theme.monospaceFontFamily
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                        Layout.preferredWidth: 56
                    }

                    Label {
                        Layout.fillWidth: true
                        text: message.preview
                        color: Ui.Theme.primaryText
                        font.family: Ui.Theme.monospaceFontFamily
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                text: "Waiting for data…"
                color: Ui.Theme.disabledText
                font.pixelSize: 13
                visible: logView.count === 0
            }
        }
    }
}
