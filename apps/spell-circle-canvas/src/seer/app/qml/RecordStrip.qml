// WRITING A WIRE DOWN AND PLAYING IT BACK, and the one line the session
// has to say for itself.
//
// A replay names the URI it opens the file onto rather than opening a
// wire of its own: everything above — the list, the readings, the log —
// then reads the file exactly as it read the port, which is the whole
// point of recording one.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Ifrit.Ui 1.0 as Ui

Ui.GlassPanel {
    id: strip

    required property var session

    radius: 12

    // The ground the pane stands on, rounded to the panel's own corners
    // and outlined, so a pane reads as a pane on a window wearing the
    // machine's glass and on one painting its own opaque colour alike.
    Rectangle {
        anchors.fill: parent
        color: Ui.Theme.toolbarBackground
        radius: strip.radius
        border.width: 1
        border.color: Ui.Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 8

        Button {
            text: "Record to File…"
            enabled: strip.session.reading.present && !strip.session.recording
            onClicked: recordDialog.open()
        }

        Button {
            text: "Stop"
            enabled: strip.session.recording
            onClicked: strip.session.stopRecording()
        }

        ToolSeparator {}

        TextField {
            id: replayUri

            Layout.preferredWidth: 190
            placeholderText: "udp://:27020"
            font.family: Ui.Theme.monospaceFontFamily
        }

        Button {
            text: "Replay File onto URI…"
            enabled: replayUri.text.trim().length > 0
            onClicked: replayDialog.open()
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignRight
            text: strip.session.recording ? "recording → " + strip.session.recordingPath : strip.session.note
            color: strip.session.recording ? Ui.Theme.statusText : Ui.Theme.secondaryText
            font.pixelSize: 11
            elide: Text.ElideMiddle
        }
    }

    FileDialog {
        id: recordDialog

        fileMode: FileDialog.SaveFile
        nameFilters: ["Feed recording (*.feed)"]
        defaultSuffix: "feed"
        acceptLabel: "Record"
        onAccepted: strip.session.recordTo(recordDialog.selectedFile)
    }

    FileDialog {
        id: replayDialog

        fileMode: FileDialog.OpenFile
        nameFilters: ["Feed recording (*.feed)", "Any file (*)"]
        acceptLabel: "Replay"
        onAccepted: strip.session.replay(replayUri.text.trim(), replayDialog.selectedFile)
    }
}
