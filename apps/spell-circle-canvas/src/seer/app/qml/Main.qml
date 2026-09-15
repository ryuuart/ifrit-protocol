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
import Ifrit.Ui 1.0 as Ui
import Sigil.Seer

ApplicationWindow {
    id: window

    width: 1200
    height: 800
    minimumWidth: 900
    minimumHeight: 560
    visible: true
    title: "Seer"
    color: Ui.Theme.windowBackground
    // The style's own background would paint over the vibrancy glass.
    background: null

    // The glass goes behind a native window, and a run drawing against no
    // display has none — so the session is asked first, and the window
    // keeps its opaque palette-driven ground where the answer is no.
    Component.onCompleted: {
        if (session.nativeChrome && Ui.WindowChrome.applyVibrancy(window))
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

    SeerSession {
        id: session
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            ConnectionsPane {
                session: session
                Layout.preferredWidth: 340
                Layout.minimumWidth: 260
                Layout.fillHeight: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                ReceivePane {
                    session: session
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                SendPane {
                    session: session
                    Layout.fillWidth: true
                    Layout.preferredHeight: 248
                    Layout.minimumHeight: 180
                }
            }
        }

        RecordStrip {
            session: session
            Layout.fillWidth: true
            Layout.preferredHeight: 46
        }
    }
}
