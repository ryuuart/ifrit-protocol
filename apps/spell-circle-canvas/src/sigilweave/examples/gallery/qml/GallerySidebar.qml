pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Qt 1.0 as Ui

/** Controls for an object exposing the SigilWeave GalleryView interface. */
ColumnLayout {
    id: root

    required property var view
    readonly property bool hasCustomControls: root.view.sceneControlsQml.toString() !== ""

    /** Returns a bounded, case-insensitive family search for the popup. */
    function matchingFontFamilies(query) {
        const trimmedQuery = query.trim();
        const matches = Ui.FontDatabase.searchFamilies(trimmedQuery);
        return "(scene default)".includes(trimmedQuery.toLocaleLowerCase()) ? ["(scene default)"].concat(matches).slice(0, 80) : matches.slice(0, 80);
    }

    /** Keeps compact axis readouts useful for both integer and fractional axes. */
    function formatAxisValue(value) {
        const rounded = Math.round(value);
        return Math.abs(value - rounded) < 0.01 ? rounded.toString() : value.toFixed(2);
    }

    spacing: Ui.Theme.sectionSpacing

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Ui.Theme.spacing

        Ui.SectionHeading {
            text: "SCENE"
        }
        ComboBox {
            Layout.fillWidth: true
            model: root.view.sceneNames
            currentIndex: root.view.sceneIndex
            Accessible.name: "Scene"
            onActivated: index => root.view.sceneIndex = index
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Ui.Theme.spacing

            CheckBox {
                text: "Animate"
                checked: root.view.animating
                onToggled: root.view.animating = checked
            }
            Ui.SegmentedControl {
                Layout.fillWidth: true
                model: [
                    {
                        text: "CPU"
                    },
                    {
                        text: "GPU",
                        enabled: root.view.gpuAvailable
                    }
                ]
                currentIndex: root.view.gpu ? 1 : 0
                Accessible.name: "Renderer"
                onActivated: index => root.view.gpu = index === 1
            }
        }
    }

    ScrollView {
        id: controlsScroll

        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: controlsScroll.availableWidth
            spacing: Ui.Theme.sectionSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Ui.Theme.spacing

                Ui.SectionHeading {
                    text: "TYPOGRAPHY"
                }
                Ui.FontFamilyField {
                    Layout.fillWidth: true
                    family: root.view.fontFamily.length === 0 ? "(scene default)" : root.view.fontFamily
                    searchFamilies: query => root.matchingFontFamilies(query)
                    onFamilyChosen: family => root.view.fontFamily = family === "(scene default)" ? "" : family
                }
                Ui.SliderField {
                    Layout.fillWidth: true
                    label: "Size"
                    from: 10
                    to: 200
                    decimals: 0
                    suffix: " px"
                    value: root.view.fontSize
                    onValueEdited: value => root.view.fontSize = value
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.view.fontAxes.length > 0
                spacing: Ui.Theme.spacing

                Ui.SectionHeading {
                    text: "VARIABLE AXES"
                }
                Repeater {
                    model: root.view.fontAxes

                    delegate: Ui.SliderField {
                        required property var modelData

                        Layout.fillWidth: true
                        visible: !modelData.hidden
                        label: modelData.tag
                        from: modelData.minimum
                        to: modelData.maximum
                        value: root.view.fontAxisValues[modelData.tag]
                        decimals: Math.abs(value - Math.round(value)) < 0.01 ? 0 : 2
                        resetEnabled: true
                        resetValue: modelData.defaultValue
                        onValueEdited: value => root.view.setFontAxisValue(modelData.tag, value)
                        onResetRequested: root.view.setFontAxisValue(modelData.tag, modelData.defaultValue)
                        Accessible.description: "Range " + root.formatAxisValue(from) + "–" + root.formatAxisValue(to) + "; default " + root.formatAxisValue(resetValue)
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Ui.Theme.spacing

                Ui.SectionHeading {
                    text: "PARAGRAPH"
                }
                Label {
                    text: "Alignment"
                    color: Ui.Theme.secondaryText
                    font.pixelSize: Ui.Theme.captionSize
                }
                Ui.SegmentedControl {
                    Layout.fillWidth: true
                    model: [
                        {
                            text: "Start"
                        },
                        {
                            text: "Center"
                        },
                        {
                            text: "End"
                        },
                        {
                            text: "Justify"
                        }
                    ]
                    currentIndex: root.view.alignmentIndex
                    Accessible.name: "Paragraph alignment"
                    onActivated: index => root.view.alignmentIndex = index
                }
                Label {
                    text: "Line breaker"
                    color: Ui.Theme.secondaryText
                    font.pixelSize: Ui.Theme.captionSize
                }
                Ui.SegmentedControl {
                    Layout.fillWidth: true
                    model: [
                        {
                            text: "Greedy"
                        },
                        {
                            text: "Knuth–Plass"
                        }
                    ]
                    currentIndex: root.view.lineBreakStrategyIndex
                    Accessible.name: "Line breaker"
                    onActivated: index => root.view.lineBreakStrategyIndex = index
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.hasCustomControls || root.view.sceneParameters.length > 0
                spacing: Ui.Theme.spacing

                Ui.SectionHeading {
                    text: "SCENE OPTIONS"
                }
                Loader {
                    Layout.fillWidth: true
                    active: root.hasCustomControls
                    visible: active
                    source: root.view.sceneControlsQml
                    onLoaded: item.view = root.view
                }
                Repeater {
                    model: root.hasCustomControls ? [] : root.view.sceneParameters

                    delegate: SceneParameterControl {
                        required property var modelData

                        view: root.view
                        parameter: modelData
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.view.textEditable
                spacing: Ui.Theme.spacing

                Ui.SectionHeading {
                    text: "LIVE TEXT"
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 170

                    TextArea {
                        id: editor
                        wrapMode: TextArea.Wrap
                        placeholderText: "Type to replace the scene's text…"
                        Accessible.name: "Scene text"
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
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Ui.Theme.spacing

        Ui.StatusIndicator {
            Layout.fillWidth: true
            text: root.view.animating ? "Animating" : "Paused"
            tone: root.view.animating ? "good" : "neutral"
        }
        Label {
            Layout.fillWidth: true
            text: root.view.stats
            color: Ui.Theme.secondaryText
            font.family: Ui.Theme.monospaceFontFamily
            font.pixelSize: Ui.Theme.captionSize
            wrapMode: Text.Wrap
        }
    }
}
