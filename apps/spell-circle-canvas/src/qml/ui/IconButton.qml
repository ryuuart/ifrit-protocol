import QtQuick

/** Compact hoverable toolbar button with a text or symbol glyph. */
Rectangle {
    id: root

    required property string text
    property real textSize: 12
    property real horizontalPadding: 12
    property color normalForegroundColor: Theme.secondaryText
    property color hoverForegroundColor: "#dddddd"
    property color normalBackgroundColor: "#2d2d2d"
    property color hoverBackgroundColor: "#404040"
    readonly property color foregroundColor: hoverHandler.hovered ? hoverForegroundColor : normalForegroundColor
    readonly property color backgroundColor: hoverHandler.hovered ? hoverBackgroundColor : normalBackgroundColor

    signal clicked

    implicitWidth: Math.max(28, label.implicitWidth + horizontalPadding)
    implicitHeight: 24
    radius: 3
    color: backgroundColor

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.foregroundColor
        font.pixelSize: root.textSize
    }

    HoverHandler {
        id: hoverHandler
    }

    TapHandler {
        onTapped: root.clicked()
    }
}
