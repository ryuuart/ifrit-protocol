pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform
import Ifrit.Qt 1.0 as Ui
import Sigil.Seer

Ui.AppWindow {
    id: window
    required property SeerSession session

    required property TextureSources textures
    property int workspace: 0
    property string textureName: ""
    property string textureApplication: ""

    function showSceneReceiver() {
        workspace = 1;
        window.session.openReceiver();
    }

    width: 1320
    height: 800
    minimumWidth: 1000
    minimumHeight: 640
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
                Action { text: "Open Scene Receiver"; onTriggered: window.showSceneReceiver() }
                Action {
                    text: "Scene Receiver Settings…"
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
                    text: "Open Scene Receiver"
                    onTriggered: window.showSceneReceiver()
                }
                Platform.MenuItem {
                    text: "Scene Receiver Settings…"
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

    Shortcut { sequence: "Ctrl+1"; onActivated: window.workspace = 0 }
    Shortcut { sequence: "Ctrl+2"; onActivated: window.workspace = 1 }
    Shortcut { sequence: "Ctrl+3"; onActivated: window.workspace = 2 }

    ReceiverSettings {
        id: receiverSettings
        receiver: window.session.receiver
        transientParent: window
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Ui.Theme.sectionSpacing
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            ColumnLayout {
                spacing: 2
                Label {
                    text: "Seer"
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                    color: Ui.Theme.primaryText
                }
                Label {
                    text: "Inspect connections, scenes and shared frames"
                    font.pixelSize: Ui.Theme.captionSize
                    color: Ui.Theme.secondaryText
                }
            }
            Item { Layout.fillWidth: true }
            Ui.SegmentedControl {
                currentIndex: window.workspace
                model: [{text: "Messages"}, {text: "Scenes"}, {text: "Textures"}]
                onActivated: index => window.workspace = index
            }
        }

        Ui.Notice {
            Layout.fillWidth: true
            visible: window.session.note.length > 0 && (window.workspace !== 0 || window.session.selected < 0)
            text: window.session.note
            tone: "warning"
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Loader {
                anchors.fill: parent
                active: window.session.receiver.opened
                z: window.workspace === 1 ? 1 : -1
                enabled: window.workspace === 1
                sourceComponent: ReceiverPane {
                    receiver: window.session.receiver
                    onSettingsRequested: receiverSettings.edit()
                }
            }

            Ui.Panel {
                anchors.fill: parent
                visible: window.workspace === 1 && !window.session.receiver.opened
                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(540, parent.width - 64)
                    spacing: 16
                    Ui.SectionHeading { text: "SPELLCIRCLE SCENES"; Layout.alignment: Qt.AlignHCenter }
                    Label {
                        Layout.fillWidth: true
                        text: "Receive a scene. See it live."
                        font.pixelSize: 30
                        font.weight: Font.DemiBold
                        color: Ui.Theme.primaryText
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "Preview incoming vector scenes, inspect their activity and publish the canvas as a shared texture. The source stays independent of the message connection you are inspecting."
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: Ui.Theme.bodySize
                        color: Ui.Theme.secondaryText
                    }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        Button { text: "Open Scene Receiver"; highlighted: true; onClicked: window.showSceneReceiver() }
                        Button { text: "Settings…"; onClicked: receiverSettings.edit() }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "No ports are opened until you start receiving."
                        horizontalAlignment: Text.AlignHCenter
                        color: Ui.Theme.secondaryText
                        font.pixelSize: Ui.Theme.captionSize
                    }
                }
            }

            SplitView {
                anchors.fill: parent
                visible: window.workspace === 0
                orientation: Qt.Horizontal
                ConnectionsPane {
                    id: connectionPane
                    session: window.session
                    SplitView.preferredWidth: 310
                    SplitView.minimumWidth: 260
                }
                ColumnLayout {
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 530
                    spacing: 12
                    Ui.Panel {
                        visible: window.session.selected < 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        ColumnLayout {
                            anchors.centerIn: parent
                            width: Math.min(480, parent.width - 64)
                            spacing: 14
                            Ui.SectionHeading { text: "START A CONNECTION" }
                            Label {
                                Layout.fillWidth: true
                                text: "Follow the message."
                                color: Ui.Theme.primaryText
                                font.pixelSize: 30
                                font.weight: Font.DemiBold
                            }
                            Label {
                                Layout.fillWidth: true
                                text: "Choose a protocol at the left, enter its address and connect. Incoming messages appear as text, JSON, bytes or their native format."
                                wrapMode: Text.WordWrap
                                color: Ui.Theme.secondaryText
                                font.pixelSize: Ui.Theme.bodySize
                            }
                            Ui.FactRow { Layout.fillWidth: true; labelWidth: 132; label: "Listen locally"; value: "udp://:27020"; monospace: true }
                            Ui.FactRow { Layout.fillWidth: true; labelWidth: 132; label: "OSC messages"; value: "osc://:27050"; monospace: true }
                            Ui.FactRow { Layout.fillWidth: true; labelWidth: 132; label: "WebSocket"; value: "ws://:27060/sky"; monospace: true }
                            Label {
                                Layout.fillWidth: true
                                text: "Use Scenes for SpellCircle diagrams or Textures for live Syphon publications."
                                wrapMode: Text.WordWrap
                                color: Ui.Theme.secondaryText
                                font.pixelSize: Ui.Theme.captionSize
                            }
                        }
                    }
                    ReceivePane {
                        visible: window.session.selected >= 0
                        session: window.session
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }
                    SendPane {
                        visible: window.session.selected >= 0
                        session: window.session
                        Layout.fillWidth: true
                        Layout.preferredHeight: 248
                        Layout.minimumHeight: 180
                    }
                }
            }
            TexturesPane {
                anchors.fill: parent
                visible: window.workspace === 2
                sources: window.textures
                sourceName: window.textureName
                sourceApplication: window.textureApplication
            }
        }
        RecordStrip {
            visible: window.workspace === 0
            session: window.session
            Layout.fillWidth: true
            Layout.preferredHeight: 46
        }
    }
}
