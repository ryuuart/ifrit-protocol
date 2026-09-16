// THE WINDOW: the wires down the left, what arrives on the one being
// read above, what is sent back below it, and the recording strip
// across the foot.
//
// Reading and sending stand one above the other because they are the
// same conversation seen from both ends: a reader who has just sent
// something looks straight up for the answer. The list keeps its own
// column because a wire is chosen once and then watched.

pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform
import Ifrit.Qt 1.0 as Ui
import Sigil.Seer

ApplicationWindow {
    id: window
    required property SeerSession session

    width: session.receiver.opened ? 1600 : 1200
    height: 800
    minimumWidth: window.session.receiver.opened ? 1150 : 900
    minimumHeight: 560
    visible: true
    title: "Seer"
    color: Ui.Theme.windowBackground
    // The style's own background would paint over the vibrancy glass.
    background: null

    function closeActiveWindow() {
        if (receiverSettings.visible) receiverSettings.close();
        else window.close();
    }
    function openReceiverSettings() {
        receiverSettings.edit();
    }

    menuBar: Loader {
        active: Qt.platform.os !== "osx"
        visible: active
        sourceComponent: MenuBar {
            Menu {
                title: "File"
                Action { text: "Open Receiver"; onTriggered: window.session.openReceiver() }
                Action {
                    text: "Receiver Settings…"
                    shortcut: "Ctrl+,"
                    onTriggered: window.openReceiverSettings()
                }
                Action {
                    text: "Close Window"
                    shortcut: StandardKey.Close
                    onTriggered: window.closeActiveWindow()
                }
                Action { text: "Quit"; shortcut: StandardKey.Quit; onTriggered: Qt.quit() }
            }
        }
    }
    Loader {
        active: false
        // workaround: native menu creation must follow restoration of the
        // window frame to avoid AppKit structural-region tracking failure.
        Component.onCompleted: {
            if (Qt.platform.os === "osx") Qt.callLater(() => active = true);
        }
        sourceComponent: Platform.MenuBar {
            Platform.Menu {
                title: "File"
                Platform.MenuItem {
                    text: "Open Receiver"
                    onTriggered: window.session.openReceiver()
                }
                Platform.MenuItem {
                    text: "Receiver Settings…"
                    role: Platform.MenuItem.PreferencesRole
                    shortcut: StandardKey.Preferences
                    onTriggered: window.openReceiverSettings()
                }
                Platform.MenuItem {
                    text: "Close Window"
                    shortcut: StandardKey.Close
                    onTriggered: window.closeActiveWindow()
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
                    onTriggered: window.visibility === Window.Maximized
                        ? window.showNormal() : window.showMaximized()
                }
            }
        }
    }

    // The glass goes behind a native window, and a run drawing against no
    // display has none — so the session is asked first, and the window
    // keeps its opaque palette-driven ground where the answer is no.
    Component.onCompleted: {
        if (window.session.nativeChrome && Ui.WindowChrome.applyVibrancy(window))
            window.color = "transparent";
    }

    // Where the window was left. Nothing else is remembered: a wire is
    // opened for a session, and a tool that reopened yesterday's ports
    // on its own would be listening before anyone asked it to.
    Settings {
        category: "window"
        property alias x: window.x
        property alias y: window.y
        property alias width: window.width
        property alias height: window.height
    }

    ReceiverSettings {
        id: receiverSettings
        receiver: window.session.receiver
        transientParent: window
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            ConnectionsPane {
                session: window.session
                SplitView.preferredWidth: 280
                SplitView.minimumWidth: 220
            }

            ReceiverPane {
                visible: window.session.receiver.opened
                receiver: window.session.receiver
                SplitView.preferredWidth: 650
                SplitView.minimumWidth: 380
                onSettingsRequested: receiverSettings.edit()
            }

            ColumnLayout {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 440
                spacing: 12

                ReceivePane {
                    session: window.session
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                SendPane {
                    session: window.session
                    Layout.fillWidth: true
                    Layout.preferredHeight: 248
                    Layout.minimumHeight: 180
                }
            }
        }

        RecordStrip {
            session: window.session
            Layout.fillWidth: true
            Layout.preferredHeight: 46
        }
    }
}
