import QtCore
import QtQuick
import QtQuick.Controls
import Qt.labs.platform as Platform
import Ifrit.Ui 1.0 as Ui
import SpellCircle.Models 1.0

/** Top-level Spell Circle application composition. */
ApplicationWindow {
    id: root

    width: 1280
    height: 800
    minimumWidth: 700
    minimumHeight: 480
    visible: true
    title: "Spell Circle"
    color: Ui.Theme.windowBackground
    // The style's default background would paint over the vibrancy glass.
    background: null

    property bool sidebarOpen: true

    function openSettings() {
        settingsWindow.visible = true;
        settingsWindow.raise();
        settingsWindow.requestActivate();
    }

    /** Dismisses the settings sheet (reverting unsaved edits) when it has
     *  focus; otherwise closes the main window. */
    function closeActiveWindow() {
        if (settingsWindow.visible)
            settingsWindow.cancel();
        else
            root.close();
    }

    Component.onCompleted: {
        if (Ui.WindowChrome.applyVibrancy(root))
            root.color = "transparent";
    }

    Settings {
        category: "MainWindow"
        property alias x: root.x
        property alias y: root.y
        property alias width: root.width
        property alias height: root.height
    }

    // In-window menu bar for platforms without a global one (Windows, most
    // Linux desktops). Loaded only there so its Action shortcuts never
    // double-register with the native macOS menu below.
    menuBar: Loader {
        active: Qt.platform.os !== "osx"
        visible: active
        sourceComponent: MenuBar {
            Menu {
                title: "File"

                Action {
                    text: "Settings…"
                    shortcut: "Ctrl+,"
                    onTriggered: root.openSettings()
                }
                Action {
                    text: "Close Window"
                    shortcut: StandardKey.Close
                    onTriggered: root.closeActiveWindow()
                }
                MenuSeparator {}
                Action {
                    text: "Exit"
                    shortcut: StandardKey.Quit
                    onTriggered: Qt.quit()
                }
            }
        }
    }

    // The native menu bar is created one event-loop turn after startup:
    // instantiating it while the window is still adopting a Settings-restored
    // frame trips an AppKit over-release in structural-region tracking
    // (SIGTRAP under _NSTrackingAreaAKManager, macOS 26 + Qt 6.11).
    Loader {
        Component.onCompleted: {
            if (Qt.platform.os === "osx")
                Qt.callLater(() => active = true);
        }
        active: false
        sourceComponent: Platform.MenuBar {
            Platform.Menu {
                title: "File"

                Platform.MenuItem {
                    text: "Settings…"
                    role: Platform.MenuItem.PreferencesRole
                    shortcut: StandardKey.Preferences
                    onTriggered: root.openSettings()
                }
                Platform.MenuItem {
                    text: "Close Window"
                    shortcut: StandardKey.Close
                    onTriggered: root.closeActiveWindow()
                }
            }
            Platform.Menu {
                title: "Window"

                Platform.MenuItem {
                    text: "Minimize"
                    shortcut: "Meta+M"
                    onTriggered: root.showMinimized()
                }
                Platform.MenuItem {
                    text: "Zoom"
                    onTriggered: root.visibility === Window.Maximized ? root.showNormal() : root.showMaximized()
                }
            }
        }
    }

    SettingsWindow {
        id: settingsWindow
        config: Models.graphicsConfig
        fontDatabase: FontDatabase
        network: Models.networkManager
        visible: false
    }

    /** Round floating chrome button (sidebar toggle, settings). */
    component GlassIconButton: Rectangle {
        id: iconButton

        property alias text: iconGlyph.text
        property string tooltip
        signal clicked

        width: 30
        height: 30
        radius: 15
        color: buttonHover.hovered ? Ui.Theme.controlHoverBackground : Ui.Theme.controlBackground
        border.color: Ui.Theme.border
        border.width: 1

        Text {
            id: iconGlyph
            anchors.centerIn: parent
            color: Ui.Theme.primaryText
            font.pixelSize: 13
        }

        HoverHandler {
            id: buttonHover
        }
        TapHandler {
            onTapped: iconButton.clicked()
        }
        ToolTip.visible: buttonHover.hovered && iconButton.tooltip.length > 0
        ToolTip.delay: 600
        ToolTip.text: iconButton.tooltip
    }

    // ── Split view (HIG: Split views): sidebar column left, canvas detail
    // right, with the column toggle and settings buttons floating at the
    // detail area's top corners — mirrors the native macOS app.

    LogSidebar {
        id: logSidebar
        anchors {
            top: parent.top
            bottom: parent.bottom
            left: parent.left
        }
        open: root.sidebarOpen
        model: Models.spellCircleModel
        network: Models.networkManager
        zoomPercent: previewPane.viewScale * 100
        canvasWidth: Models.graphicsConfig.canvas.width
        canvasHeight: Models.graphicsConfig.canvas.height
        onFitRequested: previewPane.fitView()
        onActualSizeRequested: previewPane.zoomToActualSize()
        onClearRequested: Models.spellCircleModel.clear()
    }

    Rectangle {
        id: sidebarDivider
        anchors {
            top: parent.top
            bottom: parent.bottom
            left: logSidebar.right
        }
        width: root.sidebarOpen ? 1 : 0
        color: Ui.Theme.border
    }

    PreviewPane {
        id: previewPane
        anchors {
            top: parent.top
            right: parent.right
            bottom: parent.bottom
            left: sidebarDivider.right
        }
        model: Models.spellCircleModel
        config: Models.graphicsConfig
    }

    GlassIconButton {
        anchors.left: previewPane.left
        anchors.leftMargin: 12
        y: 8
        text: "☰"
        tooltip: root.sidebarOpen ? "Hide sidebar" : "Show sidebar"
        onClicked: root.sidebarOpen = !root.sidebarOpen
    }

    GlassIconButton {
        anchors.right: parent.right
        anchors.rightMargin: 12
        y: 8
        text: "⚙"
        tooltip: "Settings"
        onClicked: root.openSettings()
    }
}
