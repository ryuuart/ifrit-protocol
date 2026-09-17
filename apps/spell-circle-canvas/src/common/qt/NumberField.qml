import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Qt 1.0 as Ui

/** A precise decimal value with a label, unit and user-only edit signal. */
RowLayout {
    id: field
    required property string label
    required property real value
    property real from: 0
    property real to: 2000
    property real step: 1
    property int decimals: 0
    property string suffix: ""
    readonly property int factor: Math.pow(10, decimals)
    signal valueEdited(real value)

    spacing: Ui.Theme.spacing
    Label {
        Layout.fillWidth: true
        text: field.label
        color: Ui.Theme.primaryText
    }
    SpinBox {
        Layout.preferredWidth: 150
        objectName: "numberInput"
        Accessible.name: field.label
        editable: true
        from: Math.round(field.from * field.factor)
        to: Math.round(field.to * field.factor)
        stepSize: Math.round(field.step * field.factor)
        value: Math.round(field.value * field.factor)
        textFromValue: (value, locale) => Number(value / field.factor).toLocaleString(locale, 'f', field.decimals)
        valueFromText: (text, locale) => Math.round(Number.fromLocaleString(locale, text) * field.factor)
        validator: DoubleValidator {
            bottom: field.from
            top: field.to
            decimals: field.decimals
        }
        onValueModified: field.valueEdited(value / field.factor)
    }
    Label {
        Layout.preferredWidth: 20
        text: field.suffix
        color: Ui.Theme.secondaryText
        font.pixelSize: Ui.Theme.captionSize
    }
}
