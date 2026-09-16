import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: status

    property string text: ""
    property string tone: "neutral"
    property bool busy: false
    readonly property color toneColor: tone === "good" ? Theme.statusText
        : tone === "warning" ? Theme.warningText
        : tone === "error" ? Theme.errorText : Theme.secondaryText

    spacing: Theme.spacing
    Accessible.role: Accessible.StaticText
    Accessible.name: text
    Rectangle {
        id: dot
        implicitWidth: 7
        implicitHeight: 7
        radius: width / 2
        color: status.toneColor
        SequentialAnimation on opacity {
            running: status.busy && status.visible
            loops: Animation.Infinite
            NumberAnimation { to: 0.3; duration: 500 }
            NumberAnimation { to: 1; duration: 500 }
            onRunningChanged: if (!running) dot.opacity = 1
        }
    }
    Label {
        id: statusLabel
        Layout.fillWidth: true
        visible: text.length > 0
        text: status.text
        color: status.toneColor
        font.pixelSize: Theme.captionSize
        elide: Text.ElideRight
        Accessible.ignored: true
        ToolTip.visible: statusHover.hovered && truncated
        ToolTip.text: status.text
        HoverHandler { id: statusHover }
    }
}
