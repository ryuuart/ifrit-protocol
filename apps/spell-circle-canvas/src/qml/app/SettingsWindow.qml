import QtQuick
import Ifrit.Ui 1.0 as Ui

/** Window shell for application-owned graphics configuration. */
Window {
    id: root

    required property var config
    required property var fontDatabase

    title: "Graphics Settings"
    width: 480
    height: 560
    minimumWidth: 460
    minimumHeight: 560
    color: Ui.Theme.panelBackground

    palette.windowText: "#dddddd"
    palette.text: "#dddddd"
    palette.buttonText: "#dddddd"
    palette.button: "#2d2d2d"
    palette.base: "#2a2a2a"

    GraphicsSettingsForm {
        anchors.fill: parent
        anchors.margins: 16
        config: root.config
        fontDatabase: root.fontDatabase
        onSaveRequested: root.config.save()
    }
}
