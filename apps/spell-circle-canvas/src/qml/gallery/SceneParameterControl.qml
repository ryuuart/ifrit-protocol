import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Ui 1.0 as Ui

/**
 * One generic sidebar control for a scene-declared parameter
 * (GalleryView.sceneParameters entry): a Switch for bools, a labelled
 * Slider for floats/ints, a ComboBox for choices. Scenes that want richer
 * controls supply their own QML via SceneDescriptor::controlsQml instead.
 */
RowLayout {
    id: control

    required property var view
    required property var parameter

    readonly property var currentValue: view.sceneParameterValues[parameter.id]

    Layout.fillWidth: true

    Switch {
        visible: control.parameter.type === "bool"
        text: control.parameter.label
        checked: control.parameter.type === "bool" ? (control.currentValue === true) : false
        onToggled: control.view.setSceneParameter(control.parameter.id, checked)
    }

    Label {
        visible: control.parameter.type === "float" || control.parameter.type === "int"
                 || control.parameter.type === "choice"
        text: control.parameter.label
        color: "#9aa3b2"
    }
    Slider {
        id: numericSlider
        visible: control.parameter.type === "float" || control.parameter.type === "int"
        Layout.fillWidth: true
        from: control.parameter.minimum
        to: control.parameter.maximum
        stepSize: control.parameter.type === "int" ? 1 : 0
        snapMode: control.parameter.type === "int" ? Slider.SnapAlways : Slider.NoSnap
        value: visible ? Number(control.currentValue) : 0
        onMoved: control.view.setSceneParameter(
            control.parameter.id,
            control.parameter.type === "int" ? Math.round(value) : value)
    }
    Label {
        visible: numericSlider.visible
        text: (control.parameter.type === "int"
               ? Math.round(numericSlider.value)
               : numericSlider.value.toFixed(1)) + control.parameter.suffix
        color: "#9aa3b2"
    }

    ComboBox {
        visible: control.parameter.type === "choice"
        Layout.fillWidth: true
        model: control.parameter.choices
        currentIndex: control.parameter.type === "choice" ? Number(control.currentValue) : 0
        onActivated: index => control.view.setSceneParameter(control.parameter.id, index)
    }
}
