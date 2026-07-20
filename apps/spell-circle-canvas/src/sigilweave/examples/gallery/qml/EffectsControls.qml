import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Ui 1.0 as Ui

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

    spacing: 8
    Layout.fillWidth: true

    ComboBox {
        Layout.fillWidth: true
        model: ["Layer showcase", "Loud shaders", "2,000-word stress"]
        currentIndex: controls.mode
        onActivated: index => controls.view.setSceneParameter("mode", index)
    }

    // Shader-pass toggles apply to the stress wall; the other modes manage
    // their own fixed paint stacks.
    RowLayout {
        visible: controls.mode === 2

        Switch {
            text: "Glow"
            checked: controls.values["glow"] === true
            onToggled: controls.view.setSceneParameter("glow", checked)
        }
        Switch {
            text: "Outline"
            checked: controls.values["outline"] === true
            onToggled: controls.view.setSceneParameter("outline", checked)
        }
    }
    RowLayout {
        visible: controls.mode === 2

        Switch {
            text: "Shader"
            checked: controls.values["shader"] === true
            onToggled: controls.view.setSceneParameter("shader", checked)
        }
        Switch {
            text: "Stars"
            checked: controls.values["stars"] === true
            onToggled: controls.view.setSceneParameter("stars", checked)
        }
    }
    RowLayout {
        visible: controls.mode === 2

        Label {
            text: "Glow spread"
            color: Ui.Theme.secondaryText
        }
        Slider {
            id: spreadSlider
            Layout.fillWidth: true
            from: 0
            to: 8
            value: Number(controls.values["glowSpread"] ?? 0.6)
            onMoved: controls.view.setSceneParameter("glowSpread", value)
        }
        Label {
            text: spreadSlider.value.toFixed(1) + "px"
            color: Ui.Theme.secondaryText
        }
    }
    RowLayout {
        visible: controls.mode === 2

        Label {
            text: "Glow intensity"
            color: Ui.Theme.secondaryText
        }
        Slider {
            id: intensitySlider
            Layout.fillWidth: true
            from: 0.2
            to: 3.0
            value: Number(controls.values["glowIntensity"] ?? 1.3)
            onMoved: controls.view.setSceneParameter("glowIntensity", value)
        }
        Label {
            text: intensitySlider.value.toFixed(1) + "×"
            color: Ui.Theme.secondaryText
        }
    }
}
