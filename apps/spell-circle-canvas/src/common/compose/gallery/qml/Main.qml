import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Compose.Gallery

ApplicationWindow {
    id: window
    width: 1220
    height: 700
    visible: true
    title: "ComposeGallery — SigilCompose stress catalog"
    color: "#0b0a14"

    // Sidebar model mirrors compose_gallery::kScenes (name/category/catalog).
    property var scenes: [
        { name: "rpg hud",      category: "Showcase",           tag: "practical UI" },
        { name: "manuscript",   category: "Showcase",           tag: "ornament" },
        { name: "nine slice",   category: "Showcase",           tag: "#9 texture-gen" },
        { name: "botanical",    category: "Showcase",           tag: "generative" },
        { name: "ui particles", category: "Showcase",           tag: "SoA scale" },
        { name: "scoreboard",   category: "Composition & data", tag: "#2" },
        { name: "slots",        category: "Composition & data", tag: "#4" },
        { name: "grid + query", category: "Composition & data", tag: "#5 #6" },
        { name: "transitions",  category: "Composition & data", tag: "#18" },
        { name: "load",         category: "Composition & data", tag: "#21" },
        { name: "headline",     category: "Animation",          tag: "#17" },
        { name: "blend",        category: "Animation",          tag: "#3" },
        { name: "chrome",       category: "Chrome & decoration", tag: "#8 #9 #10" },
        { name: "sksl border",  category: "Chrome & decoration", tag: "#11" },
        { name: "crt + bloom",  category: "Effects",            tag: "#13 #14" },
        { name: "tile map",     category: "Tiling",             tag: "#15" },
        { name: "derive",       category: "Derive",             tag: "#7 #12" }
    ]

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---- Sidebar: scene registry grouped by catalog category ----
        Rectangle {
            Layout.preferredWidth: 250
            Layout.fillHeight: true
            color: "#12101e"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Label {
                    text: "STRESS CATALOG"
                    color: "#ffb46b"
                    font.pixelSize: 15
                    font.bold: true
                }

                ListView {
                    id: sceneList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: window.scenes
                    section.property: "category"
                    section.delegate: Label {
                        required property string section
                        text: section
                        color: "#9aa4bb"
                        font.pixelSize: 11
                        font.capitalization: Font.AllUppercase
                        topPadding: 10
                        bottomPadding: 3
                    }
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: sceneList.width
                        height: 30
                        radius: 6
                        color: view.sceneIndex === index ? "#33285a"
                                                         : "transparent"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            Label {
                                text: modelData.name
                                color: "#e8ecf8"
                                font.pixelSize: 13
                                Layout.fillWidth: true
                            }
                            Label {
                                text: modelData.tag
                                color: "#7ee8ff"
                                font.pixelSize: 11
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: view.sceneIndex = index
                        }
                    }
                }

                // ---- Clock controls ----
                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        text: view.paused ? "Resume" : "Pause"
                        Layout.fillWidth: true
                        onClicked: view.paused = !view.paused
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        text: "time scale " + speed.value.toFixed(2) + "x"
                        color: "#9aa4bb"
                        font.pixelSize: 12
                    }
                    Slider {
                        id: speed
                        Layout.fillWidth: true
                        from: 0.1
                        to: 4.0
                        value: 1.0
                        onValueChanged: view.timeScale = value
                    }
                }

                // ---- Live frame metrics (the FPS measurement) ----
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: metricsLabel.implicitHeight + 16
                    radius: 8
                    color: "#0a0912"
                    Label {
                        id: metricsLabel
                        anchors.fill: parent
                        anchors.margins: 8
                        text: view.metrics
                        color: "#7ee8ff"
                        font.family: "Menlo"
                        font.pixelSize: 11
                    }
                }
            }
        }

        // ---- The compose surface ----
        ComposeGalleryView {
            id: view
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
