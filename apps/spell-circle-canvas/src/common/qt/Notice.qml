import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Qt 1.0 as Ui

Ui.Panel {
    id: notice

    property string text: ""
    property string tone: "neutral"
    property bool busy: false
    property bool dismissible: false

    signal dismissed

    padding: Ui.Theme.spacing
    backgroundColor: Ui.Theme.solidPanelBackground
    Accessible.role: Accessible.AlertMessage
    Accessible.name: notice.text

    contentItem: RowLayout {
        spacing: Ui.Theme.spacing

        Ui.StatusIndicator {
            id: status

            tone: notice.tone
            busy: notice.busy
            Accessible.ignored: true
        }
        Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: notice.text
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: notice.tone === "neutral" ? Ui.Theme.primaryText : status.toneColor
            font.pixelSize: Ui.Theme.bodySize
            Accessible.role: Accessible.StaticText
            Accessible.name: notice.text
        }
        Ui.IconButton {
            visible: notice.dismissible
            text: "×"
            tooltip: "Dismiss message"
            onClicked: notice.dismissed()
        }
    }
}
