import QtQuick
import SpellCircle.Canvas 1.0
import SpellCircle.Models 1.0

Window {
    id: root
    width: 640
    height: 480
    visible: true
    title: "Spell Circle — Feed"

    SpellCircle {
        id: spellCircle1
        width: 500
        height: 500
        model: Models.feedModel
    }

    Rectangle {
        anchors {
            top: spellCircle1.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            topMargin: 8
        }
        color: "#1a1a1a"
        radius: 4

        ListView {
            id: feedView
            anchors.fill: parent
            anchors.margins: 4
            model: Models.feedModel
            clip: true

            delegate: Item {
                width: feedView.width
                height: 36

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    spacing: 2

                    Text {
                        text: message
                        color: "#e0e0e0"
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        width: feedView.width - 16
                    }
                    Text {
                        text: timestamp + "  ·  " + source
                        color: "#888888"
                        font.pixelSize: 10
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: "#333333"
                }
            }

            Text {
                anchors.centerIn: parent
                text: "Waiting for UDP data…"
                color: "#555555"
                font.pixelSize: 14
                visible: feedView.count === 0
            }
        }
    }
}
