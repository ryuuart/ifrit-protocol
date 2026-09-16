pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform
import Ifrit.Qt 1.0 as Ui
import SigilWeave.Gallery

/** Top-level composition for the interactive SigilWeave gallery. */
ApplicationWindow {
    id: galleryWindow

    required property int initialScene
    required property string initialText

    /** True once the native title bar accepts a subtitle; otherwise the
     *  scene name stays in the composite window title. */
    property bool nativeSubtitle: false

    readonly property string sceneName: galleryView.sceneNames[galleryView.sceneIndex]

    width: 1360
    height: 860
    minimumWidth: 860
    minimumHeight: 540
    visible: true
    title: nativeSubtitle ? "SigilWeave Gallery" : "SigilWeave Gallery — " + sceneName
    color: Ui.Theme.windowBackground

    onSceneNameChanged: {
        if (nativeSubtitle)
            Ui.WindowChrome.setSubtitle(galleryWindow, sceneName);
    }

    Component.onCompleted: {
        if (Ui.WindowChrome.applyVibrancy(galleryWindow))
            galleryWindow.color = "transparent";
        nativeSubtitle = Ui.WindowChrome.setSubtitle(galleryWindow, sceneName);
    }

    Settings {
        category: "GalleryWindow"
        property alias x: galleryWindow.x
        property alias y: galleryWindow.y
        property alias width: galleryWindow.width
        property alias height: galleryWindow.height
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
                    onTriggered: galleryWindow.close()
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
                    onTriggered: galleryWindow.close()
                }
            }
            Platform.Menu {
                title: "Window"

                Platform.MenuItem {
                    text: "Minimize"
                    shortcut: "Meta+M"
                    onTriggered: galleryWindow.showMinimized()
                }
                Platform.MenuItem {
                    text: "Zoom"
                    onTriggered: galleryWindow.visibility === Window.Maximized ? galleryWindow.showNormal() : galleryWindow.showMaximized()
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Ui.Theme.sectionSpacing
        spacing: Ui.Theme.sectionSpacing

        Ui.Panel {
            Layout.preferredWidth: 356
            Layout.minimumWidth: 356
            Layout.maximumWidth: 356
            Layout.fillHeight: true

            GallerySidebar {
                anchors.fill: parent
                view: galleryView
            }
        }

        Ui.GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Ui.Theme.panelRadius

            Rectangle {
                anchors.fill: parent
                color: Ui.Theme.canvasBackground
            }

            GalleryView {
                id: galleryView
                anchors.fill: parent

                Component.onCompleted: {
                    sceneIndex = galleryWindow.initialScene;
                    if (galleryWindow.initialText.length > 0)
                        sceneText = galleryWindow.initialText;
                }
            }
        }
    }
}
