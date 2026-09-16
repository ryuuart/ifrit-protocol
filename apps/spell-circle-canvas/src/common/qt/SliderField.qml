import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: field

    required property string label
    property real value: 0
    property real from: 0
    property real to: 1
    property real stepSize: 0
    property int decimals: 1
    property string suffix: ""
    property bool resetEnabled: false
    property real resetValue: 0
    signal valueEdited(real value)
    signal resetRequested

    spacing: Theme.spacing
    Label {
        text: field.label
        color: Theme.secondaryText
        font.pixelSize: Theme.captionSize
    }
    Slider {
        id: slider
        Layout.fillWidth: true
        Layout.minimumWidth: 48
        from: field.from
        to: field.to
        value: field.value
        stepSize: field.stepSize
        snapMode: stepSize > 0 ? Slider.SnapAlways : Slider.NoSnap
        Accessible.name: field.label
        onMoved: field.valueEdited(value)
    }
    Label {
        text: slider.value.toFixed(field.decimals) + field.suffix
        color: Theme.primaryText
        font.family: Theme.monospaceFontFamily
        font.pixelSize: Theme.captionSize
        horizontalAlignment: Text.AlignRight
        Layout.minimumWidth: 42
    }
    IconButton {
        visible: field.resetEnabled
        text: "↺"
        tooltip: "Reset " + field.label
        enabled: field.value !== field.resetValue
        onClicked: field.resetRequested()
    }
}
