import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SpellCircle.Models 1.0

Window {
    id: settingsWindow
    title: "Graphics Settings"
    width: 480
    height: 560
    minimumWidth: 460
    minimumHeight: 560
    color: "#1e1e1e"

    palette.windowText: "#dddddd"
    palette.text: "#dddddd"
    palette.buttonText: "#dddddd"
    palette.button: "#2d2d2d"
    palette.base: "#2a2a2a"

    readonly property var config: Models.graphicsConfig

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // Two-column label/control grid — the standard Qt Quick Controls form
        // layout. Related width+height pairs (Canvas Size, Box Size) share a
        // single row instead of consuming one row each.
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 14
            rowSpacing: 10

            Label { text: "Canvas Size"; Layout.preferredWidth: 100 }
            RowLayout {
                spacing: 6
                SpinBox {
                    from: 256
                    to: 8192
                    stepSize: 100
                    editable: true
                    Layout.preferredWidth: 140
                    value: settingsWindow.config.canvas.width
                    onValueModified: settingsWindow.config.canvas.width = value
                }
                Label { text: "×" }
                SpinBox {
                    from: 256
                    to: 8192
                    stepSize: 100
                    editable: true
                    Layout.preferredWidth: 140
                    value: settingsWindow.config.canvas.height
                    onValueModified: settingsWindow.config.canvas.height = value
                }
            }

            Label { text: "Stroke Width"; Layout.preferredWidth: 100 }
            SpinBox {
                from: 1
                to: 64
                editable: true
                Layout.preferredWidth: 140
                value: Math.round(settingsWindow.config.strokeWidth)
                onValueModified: settingsWindow.config.strokeWidth = value
            }

            Label { text: "Scale (%)"; Layout.preferredWidth: 100 }
            SpinBox {
                from: 10
                to: 400
                stepSize: 5
                editable: true
                Layout.preferredWidth: 140
                value: Math.round(settingsWindow.config.scale * 100)
                onValueModified: settingsWindow.config.scale = value / 100.0
            }

            Label { text: "Label Offset"; Layout.preferredWidth: 100 }
            SpinBox {
                from: -200
                to: 200
                editable: true
                Layout.preferredWidth: 140
                value: Math.round(settingsWindow.config.labelOffset)
                onValueModified: settingsWindow.config.labelOffset = value
            }

            Label { text: "Box Size"; Layout.preferredWidth: 100 }
            RowLayout {
                spacing: 6
                SpinBox {
                    from: 0
                    to: 4000
                    editable: true
                    Layout.preferredWidth: 140
                    value: Math.round(settingsWindow.config.box.width)
                    onValueModified: settingsWindow.config.box.width = value
                }
                Label { text: "×" }
                SpinBox {
                    from: 0
                    to: 4000
                    editable: true
                    Layout.preferredWidth: 140
                    value: Math.round(settingsWindow.config.box.height)
                    onValueModified: settingsWindow.config.box.height = value
                }
            }

            Label { text: "Box Padding"; Layout.preferredWidth: 100 }
            SpinBox {
                from: 0
                to: 200
                editable: true
                Layout.preferredWidth: 140
                value: Math.round(settingsWindow.config.box.padding)
                onValueModified: settingsWindow.config.box.padding = value
            }

            Label { text: "Box Distance"; Layout.preferredWidth: 100 }
            SpinBox {
                from: 0
                to: 2000
                editable: true
                Layout.preferredWidth: 140
                value: Math.round(settingsWindow.config.box.distance)
                onValueModified: settingsWindow.config.box.distance = value
            }

            Label { text: "Color"; Layout.preferredWidth: 100 }
            RowLayout {
                spacing: 8
                Rectangle {
                    width: 24
                    height: 24
                    radius: 4
                    color: settingsWindow.config.color
                    border.color: "#444444"
                }
                TextField {
                    Layout.preferredWidth: 100
                    text: settingsWindow.config.color.toString()
                    onEditingFinished: settingsWindow.config.color = text
                }
            }

            Label { text: "Font Family"; Layout.preferredWidth: 100 }
            FontFamilyField {
                Layout.preferredWidth: 240
                family: settingsWindow.config.font.family
                onFamilyChosen: function(family) {
                    var styles = FontDatabase.styles(family)
                    var style = styles.length > 0 ? styles[0] : "Regular"
                    settingsWindow.config.font = FontDatabase.font(
                        family, style, settingsWindow.config.font.pointSize)
                }
            }

            Label { text: "Font Style"; Layout.preferredWidth: 100 }
            ComboBox {
                Layout.preferredWidth: 240
                model: FontDatabase.styles(settingsWindow.config.font.family)
                currentIndex: model.indexOf(
                    FontDatabase.styleForFont(settingsWindow.config.font))
                onActivated: settingsWindow.config.font = FontDatabase.font(
                    settingsWindow.config.font.family, currentText,
                    settingsWindow.config.font.pointSize)
            }

            Label { text: "Font Size"; Layout.preferredWidth: 100 }
            SpinBox {
                from: 4
                to: 400
                editable: true
                Layout.preferredWidth: 140
                value: settingsWindow.config.font.pointSize
                onValueModified: {
                    var f = settingsWindow.config.font
                    f.pointSize = value
                    settingsWindow.config.font = f
                }
            }
        }

        Item { Layout.fillHeight: true }

        Button {
            text: "Save"
            onClicked: settingsWindow.config.save()
        }
    }
}
