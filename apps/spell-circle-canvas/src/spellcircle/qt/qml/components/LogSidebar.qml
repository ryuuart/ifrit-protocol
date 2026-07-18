import QtQuick
import QtQuick.Controls
import Ifrit.Ui 1.0 as Ui

/**
 * Split-view sidebar (HIG: Split views): connection status, viewport/perf
 * readouts, view actions, and the scene log feed — the informational column
 * beside the canvas detail area, mirroring the native macOS app.
 */
Rectangle {
    id: root

    required property var model
    required property var network
    property bool open: true
    property real expandedWidth: 280
    property real zoomPercent: 0
    property int canvasWidth: 0
    property int canvasHeight: 0
    readonly property int count: feedView.count

    signal fitRequested
    signal actualSizeRequested
    signal clearRequested

    width: open ? expandedWidth : 0
    color: Ui.Theme.panelBackground
    clip: true

    Behavior on width {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    // Fixed-width inner column so the collapse animation slides content
    // out instead of squishing it.
    Item {
        width: root.expandedWidth
        height: parent.height

        Column {
            id: statusColumn
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                topMargin: 14
                leftMargin: 16
                rightMargin: 16
            }
            spacing: 10

            Row {
                spacing: 8

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 7
                    height: 7
                    radius: 3.5
                    color: root.network.listening ? Ui.Theme.statusText : Ui.Theme.disabledText
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.network.statusText
                    color: Ui.Theme.primaryText
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }

            Column {
                width: parent.width
                spacing: 5

                Repeater {
                    model: [
                        { label: "Zoom", value: root.zoomPercent > 0 ? Math.round(root.zoomPercent) + "%" : "—" },
                        { label: "Canvas", value: root.canvasWidth + " × " + root.canvasHeight + " px" },
                        { label: "Rate", value: root.model.scenesPerSecond > 0.05 ? Math.round(root.model.scenesPerSecond) + " scenes/s" : "—" }
                    ]

                    delegate: Item {
                        required property var modelData
                        width: parent.width
                        height: 16

                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            text: parent.modelData.label
                            color: Ui.Theme.secondaryText
                            font.pixelSize: 11
                        }
                        Text {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: parent.modelData.value
                            color: Ui.Theme.primaryText
                            font.pixelSize: 11
                            font.family: Ui.Theme.monospaceFontFamily
                        }
                    }
                }
            }

            Row {
                spacing: 6

                ToolButton {
                    text: "Fit"
                    font.pixelSize: 11
                    onClicked: root.fitRequested()
                }
                ToolButton {
                    text: "100%"
                    font.pixelSize: 11
                    onClicked: root.actualSizeRequested()
                }
                ToolButton {
                    text: "Clear"
                    font.pixelSize: 11
                    enabled: root.count > 0
                    onClicked: root.clearRequested()
                }
            }
        }

        Rectangle {
            id: statusDivider
            anchors {
                top: statusColumn.bottom
                left: parent.left
                right: parent.right
                topMargin: 10
            }
            height: 1
            color: Ui.Theme.border
        }

        Item {
            id: feedHeader
            anchors {
                top: statusDivider.bottom
                left: parent.left
                right: parent.right
                leftMargin: 16
                rightMargin: 16
            }
            height: 32

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "ACTIVITY"
                color: Ui.Theme.secondaryText
                font.pixelSize: 11
                font.letterSpacing: 1.5
                font.weight: Font.DemiBold
            }

            Text {
                anchors {
                    verticalCenter: parent.verticalCenter
                    right: parent.right
                }
                text: feedView.count > 0 ? feedView.count : ""
                color: Ui.Theme.disabledText
                font.pixelSize: 11
            }
        }

        ListView {
            id: feedView
            anchors {
                top: feedHeader.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            model: root.model
            clip: true

            delegate: Item {
                id: logEntry

                required property string message
                required property string timestamp
                required property string source

                width: ListView.view.width
                height: Math.max(48, logContent.implicitHeight + 16)

                Column {
                    id: logContent
                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: 16
                        rightMargin: 12
                    }
                    spacing: 3

                    Text {
                        width: parent.width
                        text: logEntry.message
                        color: Ui.Theme.primaryText
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                        maximumLineCount: 2
                    }
                    Text {
                        text: logEntry.timestamp + " · " + logEntry.source
                        color: Ui.Theme.disabledText
                        font.pixelSize: 10
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    width: parent.width - 28
                    height: 1
                    color: Ui.Theme.border
                }
            }

            Text {
                anchors.centerIn: parent
                text: "Waiting for data…"
                color: Ui.Theme.disabledText
                font.pixelSize: 13
                visible: feedView.count === 0
            }
        }
    }
}
