import QtQuick
import Ifrit.Ui 1.0 as Ui

/** Animated log-feed sidebar with no dependency on a concrete model singleton. */
Rectangle {
    id: root

    required property var model
    property bool open: true
    property real expandedWidth: 260
    readonly property int count: feedView.count

    z: 1
    width: open ? expandedWidth : 0
    color: Ui.Theme.panelBackground
    clip: true

    Behavior on width {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    Item {
        width: root.expandedWidth
        height: parent.height

        Rectangle {
            id: header
            width: parent.width
            height: Ui.Theme.toolbarHeight
            color: Ui.Theme.toolbarBackground

            Text {
                anchors {
                    verticalCenter: parent.verticalCenter
                    left: parent.left
                    leftMargin: 12
                }
                text: "LOGS"
                color: "#aaaaaa"
                font.pixelSize: 11
                font.letterSpacing: 1.5
                font.weight: Font.DemiBold
            }

            Text {
                anchors {
                    verticalCenter: parent.verticalCenter
                    right: parent.right
                    rightMargin: 10
                }
                text: feedView.count > 0 ? feedView.count : ""
                color: "#555555"
                font.pixelSize: 11
            }
        }

        ListView {
            id: feedView
            anchors {
                top: header.bottom
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
                        leftMargin: 12
                        rightMargin: 8
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
                    width: parent.width
                    height: 1
                    color: "#2a2a2a"
                }
            }

            Text {
                anchors.centerIn: parent
                text: "Waiting for data…"
                color: "#444444"
                font.pixelSize: 13
                visible: feedView.count === 0
            }
        }
    }
}
