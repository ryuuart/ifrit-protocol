import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Compose.Gallery

ApplicationWindow {
    id: window

    width: 1440
    height: 920
    minimumWidth: 760
    minimumHeight: 560
    visible: true
    title: "ComposeGallery — SigilCompose stress catalog"
    color: "#0b0a14"

    // The Basic style paints its controls straight from the palette; without
    // this a light-grey Button and Slider sit in the middle of a dark panel.
    palette.window: "#12101e"
    palette.windowText: "#e8ecf8"
    palette.button: "#241f3d"
    palette.buttonText: "#e8ecf8"
    palette.mid: "#2f2951"
    palette.midlight: "#3a3366"
    palette.dark: "#0a0912"
    palette.light: "#3a3366"
    palette.highlight: "#5b4bb0"
    palette.text: "#e8ecf8"
    palette.base: "#0a0912"

    readonly property var stats: view.metrics

    /** One "name   value" line that gives up width by eliding the name
     *  rather than by painting past the panel. */
    component Metric: RowLayout {
        id: metricRow

        required property string name
        required property string value

        spacing: 6
        Label {
            text: metricRow.name
            color: "#767e99"
            font.pixelSize: 11
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Label {
            text: metricRow.value
            color: "#d3dbf0"
            font.family: "Menlo"
            font.pixelSize: 11
        }
    }

    // Draggable split: the catalog tags are long and the metric names are
    // fixed, so how much of the window the sidebar deserves is the reader's
    // call, not a constant.
    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        handle: Rectangle {
            implicitWidth: 5
            color: SplitHandle.pressed ? "#4a3d85"
                                       : (SplitHandle.hovered ? "#2a2350"
                                                              : "#181530")
        }

        // ---- Sidebar: scene registry grouped by catalog category ----
        Rectangle {
            SplitView.preferredWidth: 320
            SplitView.minimumWidth: 240
            SplitView.maximumWidth: 520
            color: "#12101e"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

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
                    // The metrics panel below is fixed-height; without a floor
                    // here a short window would collapse the list to nothing.
                    Layout.minimumHeight: 120
                    clip: true
                    focus: true
                    model: view.scenes
                    currentIndex: view.sceneIndex
                    onCurrentIndexChanged: {
                        view.sceneIndex = currentIndex;
                        // Arrow keys walk past the viewport otherwise: there
                        // is no `highlight` item to follow.
                        positionViewAtIndex(currentIndex, ListView.Contain);
                    }
                    keyNavigationEnabled: true
                    highlightMoveDuration: 90
                    section.property: "category"
                    section.delegate: Label {
                        required property string section
                        width: ListView.view.width
                        text: section
                        color: "#8f98b2"
                        font.pixelSize: 11
                        font.capitalization: Font.AllUppercase
                        elide: Text.ElideRight
                        topPadding: 12
                        bottomPadding: 4
                    }

                    ScrollBar.vertical: ScrollBar { id: sceneScroll }

                    // Two lines, both elided: catalog tags run to ~40
                    // characters ("EMBER GATE — the flagship living poster"),
                    // which no single-row layout can hold beside the name.
                    delegate: Rectangle {
                        id: sceneRow

                        required property var modelData
                        required property int index

                        width: sceneList.width
                               - (sceneScroll.visible ? sceneScroll.width : 0)
                        height: 42
                        radius: 7
                        color: view.sceneIndex === sceneRow.index ? "#2c2456"
                             : rowHover.hovered ? "#1b1730"
                             : "transparent"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            anchors.topMargin: 5
                            anchors.bottomMargin: 5
                            spacing: 1

                            Label {
                                text: sceneRow.modelData.name
                                color: "#e8ecf8"
                                font.pixelSize: 13
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: sceneRow.modelData.tag
                                color: "#6fc9e0"
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        HoverHandler { id: rowHover }
                        TapHandler {
                            onTapped: {
                                view.sceneIndex = sceneRow.index;
                                sceneList.forceActiveFocus();
                            }
                        }
                        ToolTip.visible: rowHover.hovered
                        ToolTip.delay: 700
                        ToolTip.text: sceneRow.modelData.name + " — "
                                      + sceneRow.modelData.tag
                    }
                }

                // ---- Clock controls ----
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Button {
                        text: view.paused ? "Resume" : "Pause"
                        Layout.fillWidth: true
                        onClicked: view.paused = !view.paused
                    }
                    Label {
                        text: speed.value.toFixed(2) + "×"
                        color: "#767e99"
                        font.family: "Menlo"
                        font.pixelSize: 11
                    }
                }
                Slider {
                    id: speed
                    Layout.fillWidth: true
                    from: 0.1
                    to: 4.0
                    value: 1.0
                    onValueChanged: view.timeScale = value
                }

                // ---- Live frame metrics (the FPS measurement) ----
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: metricsBody.implicitHeight + 24
                    Layout.minimumHeight: metricsBody.implicitHeight + 24
                    radius: 10
                    color: "#0a0912"
                    border.width: 1
                    border.color: "#1e1a33"

                    ColumnLayout {
                        id: metricsBody

                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Label {
                                text: window.stats.fps !== undefined
                                      ? window.stats.fps.toFixed(0) : "—"
                                color: "#7ee8ff"
                                font.pixelSize: 30
                                font.bold: true
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Label {
                                    text: "fps presented"
                                    color: "#767e99"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: window.stats.backend ?? ""
                                    color: "#ffb46b"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 18
                            rowSpacing: 3

                            Metric {
                                Layout.fillWidth: true
                                name: "work"
                                value: (window.stats.workMs ?? 0).toFixed(2)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "p99"
                                value: (window.stats.p99Ms ?? 0).toFixed(2)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "submit"
                                value: (window.stats.submitMs ?? 0).toFixed(2)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "headroom"
                                value: (window.stats.headroomFps ?? 0).toFixed(0)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "recon"
                                value: (window.stats.reconcileMs ?? 0).toFixed(2)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "layout"
                                value: (window.stats.layoutMs ?? 0).toFixed(2)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "volatile"
                                value: (window.stats.volatileMs ?? 0).toFixed(2)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "paint"
                                value: (window.stats.paintMs ?? 0).toFixed(2)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "instances"
                                value: String(window.stats.instances ?? 0)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "pictures"
                                value: String(window.stats.pictures ?? 0)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "textures"
                                value: String(window.stats.textures ?? 0)
                            }
                            Metric {
                                Layout.fillWidth: true
                                name: "nodes"
                                value: String(window.stats.nodes ?? 0)
                            }
                        }
                    }
                }
            }
        }

        // ---- The compose surface ----
        ComposeGalleryView {
            id: view
            SplitView.fillWidth: true
        }
    }
}
