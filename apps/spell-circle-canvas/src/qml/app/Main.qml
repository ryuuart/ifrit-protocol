import QtQuick
import Ifrit.Ui 1.0 as Ui
import SpellCircle.Models 1.0

/** Top-level Spell Circle application composition. */
Window {
    id: root

    width: 1280
    height: 800
    minimumWidth: 700
    minimumHeight: 480
    visible: true
    title: "Spell Circle"
    color: Ui.Theme.windowBackground

    property bool sidebarOpen: true

    SettingsWindow {
        id: settingsWindow
        config: Models.graphicsConfig
        fontDatabase: FontDatabase
        visible: false
    }

    LogSidebar {
        id: logSidebar
        anchors {
            top: parent.top
            bottom: parent.bottom
            left: parent.left
        }
        open: root.sidebarOpen
        model: Models.spellCircleModel
    }

    Rectangle {
        id: sidebarDivider
        anchors {
            top: parent.top
            bottom: parent.bottom
            left: logSidebar.right
        }
        width: 1
        color: Ui.Theme.border
    }

    PreviewPane {
        anchors {
            top: parent.top
            right: parent.right
            bottom: parent.bottom
            left: sidebarDivider.right
        }
        model: Models.spellCircleModel
        config: Models.graphicsConfig
        sidebarOpen: root.sidebarOpen
        logCount: logSidebar.count
        onSidebarToggled: root.sidebarOpen = !root.sidebarOpen
        onResetRequested: Models.spellCircleModel.clear()
        onSettingsRequested: {
            settingsWindow.visible = true;
            settingsWindow.raise();
            settingsWindow.requestActivate();
        }
    }
}
