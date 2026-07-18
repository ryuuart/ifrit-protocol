import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Ui 1.0 as Ui

/** Editable UDP receiver settings grouped independently from their window
 *  shell. Port edits rebind the live socket immediately. */
ColumnLayout {
    id: root

    required property var network

    spacing: 16

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 14
        rowSpacing: 10

        Label {
            text: "UDP Port"
            Layout.preferredWidth: 100
        }
        SpinBox {
            from: 1
            to: 65535
            editable: true
            Layout.preferredWidth: 140
            value: root.network.port
            onValueModified: root.network.port = value
            // Ports are identifiers, not quantities — suppress locale
            // digit grouping ("27,015").
            textFromValue: value => String(value)
            valueFromText: text => parseInt(text, 10) || root.network.port
        }

        Label {
            text: "Receiver"
            Layout.preferredWidth: 100
        }
        Switch {
            text: root.network.listening ? "Listening" : "Stopped"
            checked: root.network.listening
            onToggled: checked ? root.network.start() : root.network.stop()
        }
    }

    RowLayout {
        spacing: 8

        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: root.network.listening ? Ui.Theme.statusText : Ui.Theme.disabledText
        }
        Label {
            Layout.fillWidth: true
            text: root.network.statusText
            color: root.network.listening ? Ui.Theme.statusText : Ui.Theme.secondaryText
            font.family: Ui.Theme.monospaceFontFamily
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
