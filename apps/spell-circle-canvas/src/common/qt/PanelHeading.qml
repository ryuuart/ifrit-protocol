import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Qt 1.0 as Ui

Control {
    id: heading

    property string title: ""
    property string detail: ""
    default property alias actions: actionRow.data

    implicitHeight: contentItem.implicitHeight
    contentItem: RowLayout {
        spacing: Ui.Theme.spacing
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Ui.Theme.smallSpacing
            Label {
                Layout.fillWidth: true
                text: heading.title
                color: Ui.Theme.primaryText
                font.pixelSize: Ui.Theme.headingSize
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Accessible.role: Accessible.Heading
            }
            Label {
                Layout.fillWidth: true
                text: heading.detail
                visible: text.length > 0
                color: Ui.Theme.secondaryText
                font.pixelSize: Ui.Theme.captionSize
                elide: Text.ElideRight
                ToolTip.visible: detailHover.hovered && truncated
                ToolTip.text: text
                HoverHandler {
                    id: detailHover
                }
            }
        }
        RowLayout {
            id: actionRow
            spacing: Ui.Theme.smallSpacing
        }
    }
}
