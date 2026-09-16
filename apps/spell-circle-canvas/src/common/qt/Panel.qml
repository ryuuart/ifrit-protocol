import QtQuick
import QtQuick.Controls

Pane {
    id: panel

    property real radius: Theme.panelRadius
    property color backgroundColor: Theme.panelBackground
    property color borderColor: Theme.border

    padding: Theme.sectionSpacing
    background: Rectangle {
        color: panel.backgroundColor
        radius: panel.radius
        border.color: panel.borderColor
        border.width: 1
    }
}
