import QtQuick
import Ifrit.Ui 1.0 as Ui
import SpellCircle.Canvas 1.0

/** Spell Circle preview chrome around the reusable pan-and-zoom viewport. */
Item {
    id: root

    required property var model
    required property var config
    property bool sidebarOpen: true
    property int logCount: 0

    signal sidebarToggled
    signal settingsRequested
    signal resetRequested

    Rectangle {
        id: topBar
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        height: Ui.Theme.toolbarHeight
        color: Ui.Theme.toolbarBackground
        z: 1

        Ui.IconButton {
            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
                leftMargin: 8
            }
            text: root.sidebarOpen ? "◀" : "▶"
            textSize: 11
            onClicked: root.sidebarToggled()
        }

        Text {
            anchors.centerIn: parent
            text: "Preview"
            color: Ui.Theme.disabledText
            font.pixelSize: 12
        }

        Ui.IconButton {
            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right
                rightMargin: 8
            }
            text: "⚙"
            textSize: 13
            onClicked: root.settingsRequested()
        }
    }

    Ui.PanZoomCanvas {
        id: canvasViewport
        anchors {
            top: topBar.bottom
            left: parent.left
            right: parent.right
            bottom: bottomBar.top
        }
        canvasWidth: root.config.canvas.width
        canvasHeight: root.config.canvas.height

        SpellCircle {
            anchors.fill: parent
            clip: true
            alphaBlending: true
            fillColor: "transparent"
            model: root.model
            config: root.config
        }
    }

    Rectangle {
        z: 2
        anchors {
            right: parent.right
            bottom: bottomBar.top
            rightMargin: 14
            bottomMargin: 14
        }
        width: resetText.width + 18
        height: 28
        radius: 5
        color: resetHover.hovered ? "#3c3c3c" : Ui.Theme.toolbarBackground
        visible: root.logCount > 0

        Text {
            id: resetText
            anchors.centerIn: parent
            text: "Reset"
            color: resetHover.hovered ? "#dddddd" : Ui.Theme.secondaryText
            font.pixelSize: 11
        }

        HoverHandler {
            id: resetHover
        }
        TapHandler {
            onTapped: root.resetRequested()
        }
    }

    Rectangle {
        id: bottomBar
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: 32
        color: Ui.Theme.toolbarBackground

        Text {
            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
                leftMargin: 12
            }
            text: root.config.canvas.width + " × " + root.config.canvas.height + " px"
            color: "#4a4a4a"
            font.pixelSize: 11
            font.family: Ui.Theme.monospaceFontFamily
        }

        Row {
            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right
                rightMargin: 12
            }
            spacing: 6

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: 46
                text: Math.round(canvasViewport.viewScale * 100) + "%"
                color: Ui.Theme.secondaryText
                font.pixelSize: 12
                font.family: Ui.Theme.monospaceFontFamily
                horizontalAlignment: Text.AlignRight
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 1
                height: 16
                color: "#353535"
            }

            Ui.IconButton {
                anchors.verticalCenter: parent.verticalCenter
                height: 22
                text: "Fit"
                textSize: 11
                normalForegroundColor: "#aaaaaa"
                normalBackgroundColor: "#2a2a2a"
                hoverBackgroundColor: "#3c3c3c"
                onClicked: canvasViewport.fitView()
            }

            Ui.IconButton {
                anchors.verticalCenter: parent.verticalCenter
                height: 22
                text: "100%"
                textSize: 11
                normalForegroundColor: "#aaaaaa"
                normalBackgroundColor: "#2a2a2a"
                hoverBackgroundColor: "#3c3c3c"
                onClicked: canvasViewport.zoomToActualSize()
            }
        }
    }
}
