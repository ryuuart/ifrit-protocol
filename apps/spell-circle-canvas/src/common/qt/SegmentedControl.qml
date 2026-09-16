pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Control {
    id: control

    property var model: []
    property int currentIndex: 0
    signal activated(int index)

    padding: 3
    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    implicitHeight: Theme.controlHeight
    background: Rectangle {
        color: Theme.controlBackground
        radius: Theme.cornerRadius
        border.width: 1
        border.color: Theme.separator
    }
    contentItem: RowLayout {
        spacing: 2
        Repeater {
            model: control.model
            delegate: ToolButton {
                id: segment
                required property int index
                required property var modelData
                Layout.fillWidth: true
                Layout.fillHeight: true
                implicitWidth: implicitContentWidth + 20
                text: modelData.text
                enabled: modelData.enabled !== false
                checked: control.currentIndex === index
                font.pixelSize: Theme.captionSize
                focusPolicy: Qt.StrongFocus
                Accessible.role: Accessible.PageTab
                Accessible.name: text
                Accessible.checked: checked
                Accessible.selectable: true
                Accessible.selected: checked
                opacity: enabled ? 1 : 0.45
                onClicked: control.activated(index)
                background: Rectangle {
                    radius: Theme.cornerRadius - 2
                    color: segment.checked ? Theme.solidPanelBackground
                        : segment.hovered ? Theme.controlHoverBackground : "transparent"
                    border.width: 1
                    border.color: segment.visualFocus ? Theme.accent
                        : segment.checked ? Theme.border : "transparent"
                }
            }
        }
    }
}
