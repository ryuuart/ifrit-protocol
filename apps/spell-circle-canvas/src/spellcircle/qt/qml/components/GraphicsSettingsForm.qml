import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import Ifrit.Qt 1.0 as Ui

/** Scene styling controls; the host owns preview, persistence and cancellation. */
ColumnLayout {
    id: root

    required property var config
    required property var fontDatabase
    property int currentSection: 0

    spacing: Ui.Theme.sectionSpacing

    Ui.SegmentedControl {
        Layout.fillWidth: true
        model: [
            {
                text: "Appearance"
            },
            {
                text: "Labels"
            },
            {
                text: "Layout"
            }
        ]
        currentIndex: root.currentSection
        onActivated: index => root.currentSection = index
    }

    Ui.Panel {
        Layout.fillWidth: true
        visible: root.currentSection === 0
        ColumnLayout {
            anchors.fill: parent
            spacing: Ui.Theme.sectionSpacing
            Ui.PanelHeading {
                Layout.fillWidth: true
                title: "Ink & weight"
                detail: "Applied to every circle, edge, label and box."
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Ui.Theme.spacing
                Label {
                    text: "Scene color"
                    Layout.fillWidth: true
                }
                Basic.Button {
                    id: colorButton
                    Accessible.name: "Choose scene color"
                    implicitWidth: Ui.Theme.controlHeight
                    implicitHeight: Ui.Theme.controlHeight
                    padding: 5
                    background: Rectangle {
                        radius: Ui.Theme.cornerRadius
                        color: colorButton.hovered ? Ui.Theme.controlHoverBackground : Ui.Theme.controlBackground
                        border.color: colorButton.visualFocus ? Ui.Theme.accent : Ui.Theme.border
                    }
                    contentItem: Rectangle {
                        color: root.config.color
                        radius: Ui.Theme.smallSpacing
                        border.color: Ui.Theme.border
                    }
                    onClicked: {
                        colorDialog.selectedColor = root.config.color;
                        colorDialog.open();
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: "Choose scene color"
                }
                TextField {
                    Layout.preferredWidth: 135
                    Accessible.name: "Scene color hexadecimal value"
                    text: root.config.color.toString()
                    font.family: Ui.Theme.monospaceFontFamily
                    validator: RegularExpressionValidator {
                        regularExpression: /#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?/
                    }
                    onEditingFinished: {
                        if (acceptableInput)
                            root.config.color = text;
                        text = root.config.color.toString();
                    }
                }
            }
            Ui.NumberField {
                Layout.fillWidth: true
                suffix: "px"
                label: "Stroke width"
                value: root.config.strokeWidth
                from: 0.5
                to: 100
                step: 0.5
                decimals: 1
                onValueEdited: value => root.config.strokeWidth = value
            }
            Ui.NumberField {
                Layout.fillWidth: true
                label: "Style scale"
                value: root.config.scale * 100
                from: 5
                to: 1600
                step: 5
                suffix: "%"
                onValueEdited: value => root.config.scale = value / 100
            }
            Label {
                Layout.fillWidth: true
                text: "Scale changes strokes, type and boxes together. The incoming scene still fits the canvas."
                color: Ui.Theme.secondaryText
                font.pixelSize: Ui.Theme.captionSize
                wrapMode: Text.Wrap
            }
        }
    }

    Ui.Panel {
        Layout.fillWidth: true
        visible: root.currentSection === 1
        ColumnLayout {
            anchors.fill: parent
            spacing: Ui.Theme.sectionSpacing
            Ui.PanelHeading {
                Layout.fillWidth: true
                title: "Typography"
                detail: "One typeface for the whole scene."
            }
            Ui.FontSelector {
                Layout.fillWidth: true
                labelWidth: 86
                fieldWidth: Math.max(150, root.width - 160)
                fontDatabase: root.fontDatabase
                selectedFont: root.config.font
                minimumPointSize: 1
                maximumPointSize: 512
                onFontModified: fontValue => root.config.font = fontValue
            }
        }
    }

    Ui.Panel {
        Layout.fillWidth: true
        visible: root.currentSection === 1
        ColumnLayout {
            anchors.fill: parent
            spacing: Ui.Theme.sectionSpacing
            Ui.PanelHeading {
                Layout.fillWidth: true
                title: "Label placement"
                detail: "Distances are measured outward from the diagram."
            }
            Ui.NumberField {
                Layout.fillWidth: true
                suffix: "px"
                label: "Ring label offset"
                value: root.config.labelOffset
                from: -1000
                to: 1000
                onValueEdited: value => root.config.labelOffset = value
            }
            Ui.NumberField {
                Layout.fillWidth: true
                suffix: "px"
                label: "Point label distance"
                value: root.config.pointDistance
                from: -2000
                to: 2000
                onValueEdited: value => root.config.pointDistance = value
            }
        }
    }

    Ui.Panel {
        Layout.fillWidth: true
        visible: root.currentSection === 2
        ColumnLayout {
            anchors.fill: parent
            spacing: Ui.Theme.sectionSpacing
            Ui.PanelHeading {
                Layout.fillWidth: true
                title: "Output canvas"
                detail: "Native pixels, including the Syphon output."
            }
            Ui.DimensionSpinBoxes {
                Layout.fillWidth: true
                fieldWidth: Math.max(100, (root.width - 80) / 2)
                from: root.config.canvas.minimum
                to: root.config.canvas.maximum
                stepSize: 100
                widthValue: root.config.canvas.width
                heightValue: root.config.canvas.height
                onWidthModified: value => root.config.canvas.width = value
                onHeightModified: value => root.config.canvas.height = value
            }
        }
    }

    Ui.Panel {
        Layout.fillWidth: true
        visible: root.currentSection === 2
        ColumnLayout {
            anchors.fill: parent
            spacing: Ui.Theme.sectionSpacing
            Ui.PanelHeading {
                Layout.fillWidth: true
                title: "Label boxes"
                detail: "Boxes widen when their labels need more space."
            }
            Ui.DimensionSpinBoxes {
                Layout.fillWidth: true
                fieldWidth: Math.max(100, (root.width - 80) / 2)
                from: 0
                to: 4000
                widthValue: Math.round(root.config.box.width)
                heightValue: Math.round(root.config.box.height)
                onWidthModified: value => root.config.box.width = value
                onHeightModified: value => root.config.box.height = value
            }
            Ui.NumberField {
                Layout.fillWidth: true
                suffix: "px"
                label: "Inner padding"
                value: root.config.box.padding
                to: 500
                onValueEdited: value => root.config.box.padding = value
            }
            Ui.NumberField {
                Layout.fillWidth: true
                suffix: "px"
                label: "Distance from point"
                value: root.config.box.distance
                onValueEdited: value => root.config.box.distance = value
            }
        }
    }

    ColorDialog {
        id: colorDialog
        title: "Scene color"
        onAccepted: root.config.color = selectedColor
    }
}
