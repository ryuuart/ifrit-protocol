import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: row

    required property string label
    required property string value
    property bool monospace: true
    property int valueElide: Text.ElideMiddle
    property color valueColor: Theme.primaryText
    property real labelWidth: 80

    spacing: Theme.spacing
    Label {
        Layout.preferredWidth: row.labelWidth
        text: row.label
        color: Theme.secondaryText
        font.pixelSize: Theme.captionSize
        elide: Text.ElideRight
    }
    Label {
        Layout.fillWidth: true
        text: row.value
        color: row.valueColor
        font.family: row.monospace ? Theme.monospaceFontFamily : Qt.application.font.family
        font.pixelSize: Theme.captionSize
        elide: row.valueElide
        HoverHandler { id: valueHover }
        ToolTip.visible: truncated && valueHover.hovered
        ToolTip.text: row.value
        ToolTip.delay: 600
    }
}
