import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TextFlow.Gallery

ApplicationWindow {
    id: window
    width: 1360
    height: 860
    visible: true
    title: "TextFlow Gallery — " + view.sceneNames[view.sceneIndex]
    color: "#1d1f24"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        // ── Control panel ────────────────────────────────────────────────
        ColumnLayout {
            Layout.preferredWidth: 320
            Layout.minimumWidth: 320
            Layout.maximumWidth: 320
            Layout.fillHeight: true
            spacing: 10

            Label {
                text: "Scene"
                color: "#9aa3b2"
            }
            ComboBox {
                Layout.fillWidth: true
                model: view.sceneNames
                currentIndex: view.sceneIndex
                onActivated: index => view.sceneIndex = index
            }

            RowLayout {
                Switch {
                    text: "Animate"
                    checked: view.animating
                    onToggled: view.animating = checked
                }
                Switch {
                    text: "GPU"
                    visible: view.gpuAvailable
                    checked: view.gpu
                    onCheckedChanged: view.gpu = checked
                }
            }

            Label {
                text: "Font"
                color: "#9aa3b2"
            }
            ComboBox {
                id: fontBox
                Layout.fillWidth: true
                editable: true
                model: ["(scene default)", "Noto Sans", "Noto Serif",
                        "Helvetica Neue", "Georgia", "Avenir Next", "Menlo",
                        "Hoefler Text", "Hiragino Mincho ProN",
                        "Hiragino Sans", "Baskerville", "Futura"]
                onActivated: index =>
                    view.fontFamily = index === 0 ? "" : model[index]
                onAccepted: view.fontFamily =
                    editText === "(scene default)" ? "" : editText
            }

            RowLayout {
                Label { text: "Size"; color: "#9aa3b2" }
                Slider {
                    id: sizeSlider
                    Layout.fillWidth: true
                    from: 10; to: 40; value: view.fontSize
                    onValueChanged: view.fontSize = value
                }
                Label {
                    text: Math.round(sizeSlider.value) + "px"
                    color: "#9aa3b2"
                }
            }

            RowLayout {
                Label { text: "Weight"; color: "#9aa3b2" }
                Slider {
                    id: weightSlider
                    Layout.fillWidth: true
                    from: 50; to: 900; stepSize: 50
                    value: view.fontWeight < 100 ? 50 : view.fontWeight
                    onValueChanged: view.fontWeight = value < 100 ? 0 : value
                }
                Label {
                    text: weightSlider.value < 100
                          ? "auto" : Math.round(weightSlider.value).toString()
                    color: "#9aa3b2"
                }
            }

            Label { text: "Alignment"; color: "#9aa3b2" }
            ComboBox {
                Layout.fillWidth: true
                model: ["Start", "Center", "End", "Justify"]
                currentIndex: view.alignmentIndex
                onActivated: index => view.alignmentIndex = index
            }

            Label { text: "Line breaker"; color: "#9aa3b2" }
            ComboBox {
                Layout.fillWidth: true
                model: ["Greedy", "Knuth–Plass"]
                currentIndex: view.breakerIndex
                onActivated: index => view.breakerIndex = index
            }

            Label {
                text: "Text (live)"
                color: "#9aa3b2"
                visible: view.textEditable
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: view.textEditable
                TextArea {
                    id: editor
                    wrapMode: TextArea.Wrap
                    placeholderText: "Type to replace the scene's text…"
                    onTextChanged: if (activeFocus) view.sceneText = text
                    Connections {
                        target: view
                        function onSceneTextChanged() {
                            if (!editor.activeFocus)
                                editor.text = view.sceneText
                        }
                        function onSceneIndexChanged() {
                            editor.text = ""
                        }
                    }
                }
            }
            Item {
                Layout.fillHeight: true
                visible: !view.textEditable
            }

            Label {
                Layout.fillWidth: true
                text: view.stats
                color: "#6fbf8e"
                font.family: "Menlo"
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }

        // ── Canvas ───────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 6
            color: "#faf7f0"
            clip: true

            GalleryView {
                id: view
                anchors.fill: parent
                Component.onCompleted: {
                    sceneIndex = initialScene
                    if (initialText.length > 0)
                        sceneText = initialText
                }
            }
        }
    }
}
