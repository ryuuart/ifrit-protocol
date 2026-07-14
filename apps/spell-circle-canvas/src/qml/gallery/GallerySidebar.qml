import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Ui 1.0 as Ui

/** Controls for an object exposing the TextFlow GalleryView interface. */
ColumnLayout {
    id: root

    required property var view
    readonly property var availableFontFamilies: ["(scene default)"].concat(Qt.fontFamilies())

    /** Returns a bounded, case-insensitive family search for the popup. */
    function matchingFontFamilies(query) {
        const normalizedQuery = query.trim().toLocaleLowerCase();
        const matches = [];
        for (const family of availableFontFamilies) {
            if (normalizedQuery.length === 0 || family.toLocaleLowerCase().includes(normalizedQuery))
                matches.push(family);
            if (matches.length >= 80)
                break;
        }
        return matches;
    }

    /** Keeps compact axis readouts useful for both integer and fractional axes. */
    function formatAxisValue(value) {
        const rounded = Math.round(value);
        return Math.abs(value - rounded) < 0.01 ? rounded.toString() : value.toFixed(2);
    }

    spacing: 10

    Label {
        text: "Scene"
        color: "#9aa3b2"
    }
    ComboBox {
        Layout.fillWidth: true
        model: root.view.sceneNames
        currentIndex: root.view.sceneIndex
        onActivated: index => root.view.sceneIndex = index
    }

    RowLayout {
        Switch {
            text: "Animate"
            checked: root.view.animating
            onToggled: root.view.animating = checked
        }
        Switch {
            text: "GPU"
            visible: root.view.gpuAvailable
            checked: root.view.gpu
            onCheckedChanged: root.view.gpu = checked
        }
    }

    Label {
        text: "Font"
        color: "#9aa3b2"
    }
    Ui.FontFamilyField {
        Layout.fillWidth: true
        family: root.view.fontFamily.length === 0 ? "(scene default)" : root.view.fontFamily
        searchFamilies: query => root.matchingFontFamilies(query)
        onFamilyChosen: family => root.view.fontFamily = family === "(scene default)" ? "" : family
    }

    RowLayout {
        Label {
            text: "Size"
            color: "#9aa3b2"
        }
        Slider {
            id: sizeSlider
            Layout.fillWidth: true
            from: 10
            to: 200
            value: root.view.fontSize
            onValueChanged: root.view.fontSize = value
        }
        Label {
            text: Math.round(sizeSlider.value) + "px"
            color: "#9aa3b2"
        }
    }

    Label {
        text: "Variable axes"
        color: "#9aa3b2"
        visible: root.view.fontAxes.length > 0
    }
    Repeater {
        model: root.view.fontAxes

        delegate: RowLayout {
            required property var modelData

            Layout.fillWidth: true
            visible: !modelData.hidden

            Label {
                Layout.preferredWidth: 38
                text: modelData.tag
                color: "#9aa3b2"
                font.family: Ui.Theme.monospaceFontFamily
            }
            Slider {
                id: axisSlider
                Layout.fillWidth: true
                from: modelData.minimum
                to: modelData.maximum
                value: root.view.fontAxisValues[modelData.tag]
                onMoved: root.view.setFontAxisValue(modelData.tag, value)

                ToolTip.visible: hovered
                ToolTip.text: "Range " + root.formatAxisValue(from) + "–"
                              + root.formatAxisValue(to) + "; default "
                              + root.formatAxisValue(modelData.defaultValue)
            }
            Label {
                Layout.preferredWidth: 42
                horizontalAlignment: Text.AlignRight
                text: root.formatAxisValue(axisSlider.value)
                color: "#9aa3b2"
            }
            ToolButton {
                text: "↺"
                flat: true
                onClicked: root.view.setFontAxisValue(modelData.tag, modelData.defaultValue)
                ToolTip.visible: hovered
                ToolTip.text: "Reset " + modelData.tag
            }
        }
    }

    Label {
        text: "Alignment"
        color: "#9aa3b2"
    }
    ComboBox {
        Layout.fillWidth: true
        model: ["Start", "Center", "End", "Justify"]
        currentIndex: root.view.alignmentIndex
        onActivated: index => root.view.alignmentIndex = index
    }

    Label {
        text: "Line breaker"
        color: "#9aa3b2"
    }
    ComboBox {
        Layout.fillWidth: true
        model: ["Greedy", "Knuth–Plass"]
        currentIndex: root.view.lineBreakStrategyIndex
        onActivated: index => root.view.lineBreakStrategyIndex = index
    }

    Label {
        text: "Shader passes"
        color: "#9aa3b2"
        visible: root.view.effectTogglesSupported
    }
    RowLayout {
        visible: root.view.effectTogglesSupported

        Switch {
            text: "Glow"
            checked: root.view.effectGlow
            onToggled: root.view.effectGlow = checked
        }
        Switch {
            text: "Outline"
            checked: root.view.effectOutline
            onToggled: root.view.effectOutline = checked
        }
    }
    RowLayout {
        visible: root.view.effectTogglesSupported

        Switch {
            text: "Shader"
            checked: root.view.effectShader
            onToggled: root.view.effectShader = checked
        }
        Switch {
            text: "Stars"
            checked: root.view.effectStars
            onToggled: root.view.effectStars = checked
        }
    }
    RowLayout {
        visible: root.view.effectTogglesSupported

        Label {
            text: "Glow spread"
            color: "#9aa3b2"
        }
        Slider {
            id: glowSpreadSlider
            Layout.fillWidth: true
            from: 0
            to: 8
            value: root.view.glowSpread
            onValueChanged: root.view.glowSpread = value
        }
        Label {
            text: glowSpreadSlider.value.toFixed(1) + "px"
            color: "#9aa3b2"
        }
    }
    RowLayout {
        visible: root.view.effectTogglesSupported

        Label {
            text: "Glow intensity"
            color: "#9aa3b2"
        }
        Slider {
            id: glowIntensitySlider
            Layout.fillWidth: true
            from: 0.2
            to: 3.0
            value: root.view.glowIntensity
            onValueChanged: root.view.glowIntensity = value
        }
        Label {
            text: glowIntensitySlider.value.toFixed(1) + "×"
            color: "#9aa3b2"
        }
    }

    Label {
        text: "Text (live)"
        color: "#9aa3b2"
        visible: root.view.textEditable
    }
    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: root.view.textEditable

        TextArea {
            id: editor
            wrapMode: TextArea.Wrap
            placeholderText: "Type to replace the scene's text…"
            onTextChanged: {
                if (activeFocus)
                    root.view.sceneText = text;
            }

            Connections {
                target: root.view

                function onSceneTextChanged() {
                    if (!editor.activeFocus)
                        editor.text = root.view.sceneText;
                }

                function onSceneIndexChanged() {
                    editor.text = "";
                }
            }
        }
    }
    Item {
        Layout.fillHeight: true
        visible: !root.view.textEditable
    }

    Label {
        Layout.fillWidth: true
        text: root.view.stats
        color: Ui.Theme.statusText
        font.family: Ui.Theme.monospaceFontFamily
        font.pixelSize: 11
        wrapMode: Text.Wrap
    }
}
