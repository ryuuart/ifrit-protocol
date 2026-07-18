import QtQuick
import QtQuick.Effects

/**
 * Rounded-corner clipping container for inset content panels, matching the
 * concentric corner radii of macOS liquid-glass surfaces. Children are
 * clipped to the rounded shape (including live GPU content, which a plain
 * rectangular `clip` cannot round off).
 */
Item {
    id: root

    property real radius: 12
    default property alias content: contentItem.data

    Item {
        id: contentItem
        anchors.fill: parent
        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: maskShape
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }
    }

    Rectangle {
        id: maskShape
        anchors.fill: parent
        radius: root.radius
        visible: false
        layer.enabled: true
        layer.smooth: true
    }
}
