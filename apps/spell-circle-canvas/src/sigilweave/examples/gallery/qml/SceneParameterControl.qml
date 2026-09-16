import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Qt 1.0 as Ui

/**
 * One generic sidebar control for a scene-declared parameter
 * (GalleryView.sceneParameters entry): a checkbox for bools, a shared
 * numeric field for floats/ints, a ComboBox for choices. Scenes that want richer
 * controls supply their own QML via SceneDescriptor::controlsQml instead.
 */
RowLayout {
    id: control

    required property var view
    required property var parameter

    readonly property var currentValue: view.sceneParameterValues[parameter.id]

    Layout.fillWidth: true
    spacing: Ui.Theme.spacing

    CheckBox {
        visible: control.parameter.type === "bool"
        text: control.parameter.label
        checked: control.parameter.type === "bool" ? (control.currentValue === true) : false
        onToggled: control.view.setSceneParameter(control.parameter.id, checked)
    }

    Label {
        visible: control.parameter.type === "choice"
        text: control.parameter.label
        color: Ui.Theme.secondaryText
        font.pixelSize: Ui.Theme.captionSize
    }
    Ui.SliderField {
        visible: control.parameter.type === "float" || control.parameter.type === "int"
        Layout.fillWidth: true
        label: control.parameter.label
        from: control.parameter.minimum
        to: control.parameter.maximum
        stepSize: control.parameter.type === "int" ? 1 : 0
        decimals: control.parameter.type === "int" ? 0 : 1
        suffix: control.parameter.suffix
        value: visible ? Number(control.currentValue) : 0
        onValueEdited: value => control.view.setSceneParameter(control.parameter.id, control.parameter.type === "int" ? Math.round(value) : value)
    }

    ComboBox {
        visible: control.parameter.type === "choice"
        Layout.fillWidth: true
        model: control.parameter.choices
        Accessible.name: control.parameter.label
        currentIndex: control.parameter.type === "choice" ? Number(control.currentValue) : 0
        onActivated: index => control.view.setSceneParameter(control.parameter.id, index)
    }
}
