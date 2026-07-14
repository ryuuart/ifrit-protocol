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
            to: 40
            value: root.view.fontSize
            onValueChanged: root.view.fontSize = value
        }
        Label {
            text: Math.round(sizeSlider.value) + "px"
            color: "#9aa3b2"
        }
    }

    RowLayout {
        Label {
            text: "Weight"
            color: "#9aa3b2"
        }
        Slider {
            id: weightSlider
            Layout.fillWidth: true
            from: 50
            to: 900
            stepSize: 50
            value: root.view.fontWeight < 100 ? 50 : root.view.fontWeight
            onValueChanged: root.view.fontWeight = value < 100 ? 0 : value
        }
        Label {
            text: weightSlider.value < 100 ? "auto" : Math.round(weightSlider.value).toString()
            color: "#9aa3b2"
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
