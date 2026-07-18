import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Ifrit.Ui 1.0 as Ui

/** Editable graphics settings grouped independently from their window shell. */
ColumnLayout {
    id: root

    required property var config
    required property var fontDatabase

    spacing: 16

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 14
        rowSpacing: 10

        Label {
            text: "Canvas Size"
            Layout.preferredWidth: 100
        }
        Ui.DimensionSpinBoxes {
            from: 256
            to: 8192
            stepSize: 100
            widthValue: root.config.canvas.width
            heightValue: root.config.canvas.height
            onWidthModified: value => root.config.canvas.width = value
            onHeightModified: value => root.config.canvas.height = value
        }

        Label {
            text: "Stroke Width"
            Layout.preferredWidth: 100
        }
        SpinBox {
            from: 1
            to: 64
            editable: true
            Layout.preferredWidth: 140
            value: Math.round(root.config.strokeWidth)
            onValueModified: root.config.strokeWidth = value
        }

        Label {
            text: "Scale (%)"
            Layout.preferredWidth: 100
        }
        SpinBox {
            from: 10
            to: 400
            stepSize: 5
            editable: true
            Layout.preferredWidth: 140
            value: Math.round(root.config.scale * 100)
            onValueModified: root.config.scale = value / 100.0
        }

        Label {
            text: "Label Offset"
            Layout.preferredWidth: 100
        }
        SpinBox {
            from: -200
            to: 200
            editable: true
            Layout.preferredWidth: 140
            value: Math.round(root.config.labelOffset)
            onValueModified: root.config.labelOffset = value
        }

        Label {
            text: "Point Distance"
            Layout.preferredWidth: 100
        }
        SpinBox {
            from: -2000
            to: 2000
            editable: true
            Layout.preferredWidth: 140
            value: Math.round(root.config.pointDistance)
            onValueModified: root.config.pointDistance = value
        }

        Label {
            text: "Box Size"
            Layout.preferredWidth: 100
        }
        Ui.DimensionSpinBoxes {
            from: 0
            to: 4000
            widthValue: Math.round(root.config.box.width)
            heightValue: Math.round(root.config.box.height)
            onWidthModified: value => root.config.box.width = value
            onHeightModified: value => root.config.box.height = value
        }

        Label {
            text: "Box Padding"
            Layout.preferredWidth: 100
        }
        SpinBox {
            from: 0
            to: 200
            editable: true
            Layout.preferredWidth: 140
            value: Math.round(root.config.box.padding)
            onValueModified: root.config.box.padding = value
        }

        Label {
            text: "Box Distance"
            Layout.preferredWidth: 100
        }
        SpinBox {
            from: 0
            to: 2000
            editable: true
            Layout.preferredWidth: 140
            value: Math.round(root.config.box.distance)
            onValueModified: root.config.box.distance = value
        }

        Label {
            text: "Color"
            Layout.preferredWidth: 100
        }
        RowLayout {
            spacing: 8

            // Color well: the swatch opens the system color picker (HIG
            // "Color wells"); the field still accepts pasted hex values.
            Rectangle {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                radius: 4
                color: root.config.color
                border.color: Ui.Theme.border

                TapHandler {
                    onTapped: colorDialog.open()
                }
            }
            TextField {
                Layout.preferredWidth: 100
                text: root.config.color.toString()
                onEditingFinished: root.config.color = text
            }

            ColorDialog {
                id: colorDialog
                selectedColor: root.config.color
                onSelectedColorChanged: root.config.color = selectedColor
            }
        }
    }

    Ui.FontSelector {
        Layout.fillWidth: true
        fontDatabase: root.fontDatabase
        selectedFont: root.config.font
        onFontModified: fontValue => root.config.font = fontValue
    }

    Item {
        Layout.fillHeight: true
    }
}
