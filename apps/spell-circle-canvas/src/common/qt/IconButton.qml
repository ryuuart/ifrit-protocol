import QtQuick
import QtQuick.Controls

ToolButton {
    id: button

    property string tooltip: text

    implicitWidth: Math.max(Theme.controlHeight, implicitContentWidth + 16)
    implicitHeight: Theme.controlHeight
    font.pixelSize: Theme.bodySize
    focusPolicy: Qt.StrongFocus
    Accessible.name: tooltip
    ToolTip.visible: hovered || visualFocus
    ToolTip.delay: 600
    ToolTip.text: tooltip
}
