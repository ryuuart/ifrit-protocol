import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Qt 1.0 as Ui

/**
 * Scene-owned controls for the merged "Effects & shaders" scene — the
 * convention showcase for SceneDescriptor::controlsQml: the sidebar loads
 * this file instead of auto-building generic delegates, so the scene can
 * group its mode switch and stress-only toggles the way it wants while
 * still flowing every value through view.setSceneParameter().
 */
ColumnLayout {
    id: controls

    property var view

    readonly property var values: view ? view.sceneParameterValues : ({})
    readonly property int mode: Number(values["mode"] ?? 0)

    spacing: Ui.Theme.spacing
    Layout.fillWidth: true

    Ui.SegmentedControl {
        Layout.fillWidth: true
        model: [
            {
                text: "Layers"
            },
            {
                text: "Shaders"
            },
            {
                text: "Stress"
            }
        ]
        currentIndex: controls.mode
        Accessible.name: "Effect mode"
        onActivated: index => controls.view.setSceneParameter("mode", index)
    }
    Label {
        Layout.fillWidth: true
        text: ["Layer showcase", "Loud shaders", "2,000-word stress"][controls.mode]
        color: Ui.Theme.secondaryText
        font.pixelSize: Ui.Theme.captionSize
    }

    // Shader-pass toggles apply to the stress wall; the other modes manage
    // their own fixed paint stacks.
    RowLayout {
        visible: controls.mode === 2

        CheckBox {
            text: "Glow"
            checked: controls.values["glow"] === true
            onToggled: controls.view.setSceneParameter("glow", checked)
        }
        CheckBox {
            text: "Outline"
            checked: controls.values["outline"] === true
            onToggled: controls.view.setSceneParameter("outline", checked)
        }
    }
    RowLayout {
        visible: controls.mode === 2

        CheckBox {
            text: "Shader"
            checked: controls.values["shader"] === true
            onToggled: controls.view.setSceneParameter("shader", checked)
        }
        CheckBox {
            text: "Stars"
            checked: controls.values["stars"] === true
            onToggled: controls.view.setSceneParameter("stars", checked)
        }
    }
    Ui.SliderField {
        Layout.fillWidth: true
        visible: controls.mode === 2
        label: "Glow spread"
        from: 0
        to: 8
        suffix: " px"
        value: Number(controls.values["glowSpread"] ?? 0.6)
        onValueEdited: value => controls.view.setSceneParameter("glowSpread", value)
    }
    Ui.SliderField {
        Layout.fillWidth: true
        visible: controls.mode === 2
        label: "Glow intensity"
        from: 0.2
        to: 3.0
        suffix: "×"
        value: Number(controls.values["glowIntensity"] ?? 1.3)
        onValueEdited: value => controls.view.setSceneParameter("glowIntensity", value)
    }
}
