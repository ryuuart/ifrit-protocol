import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform
import Ifrit.Ui 1.0 as Ui
import TextFlow.Gallery

/** Top-level composition for the interactive TextFlow gallery. */
ApplicationWindow {
    id: window

    required property int initialScene
    required property string initialText

    /** True once the native title bar accepts a subtitle; otherwise the
     *  scene name stays in the composite window title. */
    property bool nativeSubtitle: false

    readonly property string sceneName: galleryView.sceneNames[galleryView.sceneIndex]

    width: 1360
    height: 860
    visible: true
    title: nativeSubtitle ? "TextFlow Gallery" : "TextFlow Gallery — " + sceneName
    color: Ui.Theme.windowBackground

    onSceneNameChanged: {
        if (nativeSubtitle)
            Ui.WindowChrome.setSubtitle(window, sceneName);
    }

    Component.onCompleted: {
        if (Ui.WindowChrome.applyVibrancy(window))
            window.color = "transparent";
        nativeSubtitle = Ui.WindowChrome.setSubtitle(window, sceneName);
    }

    Settings {
        category: "GalleryWindow"
        property alias x: window.x
        property alias y: window.y
        property alias width: window.width
        property alias height: window.height
    }

    // The style's default background would paint over the vibrancy glass.
    background: null

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
                    text: "Close Window"
                    shortcut: StandardKey.Close
                    onTriggered: window.close()
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

    // Deferred like the SpellCircle menu bar: creating the native menu bar
    // while the window adopts a Settings-restored frame trips an AppKit
    // over-release in structural-region tracking (macOS 26 + Qt 6.11).
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
                    text: "Close Window"
                    shortcut: StandardKey.Close
                    onTriggered: window.close()
                }
            }
            Platform.Menu {
                title: "Window"

                Platform.MenuItem {
                    text: "Minimize"
                    shortcut: "Meta+M"
                    onTriggered: window.showMinimized()
                }
                Platform.MenuItem {
                    text: "Zoom"
                    onTriggered: window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized()
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        GallerySidebar {
            Layout.preferredWidth: 320
            Layout.minimumWidth: 320
            Layout.maximumWidth: 320
            Layout.fillHeight: true
            view: galleryView
        }

        Ui.GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 12

            Rectangle {
                anchors.fill: parent
                color: Ui.Theme.canvasBackground
            }

            GalleryView {
                id: galleryView
                anchors.fill: parent

                Component.onCompleted: {
                    sceneIndex = window.initialScene;
                    if (window.initialText.length > 0)
                        sceneText = window.initialText;
                }
            }
        }
    }
}
