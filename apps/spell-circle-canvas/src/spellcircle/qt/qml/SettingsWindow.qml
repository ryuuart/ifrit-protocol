import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Ui 1.0 as Ui

/**
 * Configuration shell, one pane per concern. On macOS it presents as a
 * native sheet attached to the main window (HIG "Sheets"): a fixed-size
 * modal card with Done/Cancel dismissal. Elsewhere it falls back to a
 * fixed-size auxiliary dialog with the same footer.
 *
 * Edits apply to the live config immediately; Done persists them, Cancel
 * reverts to the last saved state.
 */
Window {
    id: root

    required property var config
    required property var fontDatabase
    required property var network

    /** Sheets exist on macOS only; other platforms get a dialog. */
    readonly property bool asSheet: Qt.platform.os === "osx"

    /** Per-pane window heights, System Settings style. */
    readonly property int paneHeight: networkTab.checked ? 380 : 700

    /** Persists both configurations and dismisses. */
    function done() {
        config.save();
        network.save();
        close();
    }

    /** Reverts to the last saved configurations and dismisses. */
    function cancel() {
        config.load();
        network.load();
        close();
    }

    title: "Settings"
    flags: asSheet ? Qt.Sheet : Qt.Dialog
    width: 480
    height: paneHeight
    minimumWidth: 480
    maximumWidth: 480
    minimumHeight: paneHeight
    maximumHeight: paneHeight
    // Sheets draw as native cards over the dimmed parent, so the opaque
    // palette background is correct here — no vibrancy.
    color: Ui.Theme.windowBackground

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: root.cancel()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 14

        // Checkable native buttons instead of TabBar: the macOS control style
        // has no native TabBar delegate and its fallback renders the selected
        // tab darker than the unselected one, which reads inverted.
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 4

            Button {
                id: graphicsTab
                text: "Graphics"
                checkable: true
                autoExclusive: true
                checked: true
            }
            Button {
                id: networkTab
                text: "Network"
                checkable: true
                autoExclusive: true
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Without this, the hidden tallest page dictates the layout's
            // minimum height and pushes the footer below a short pane's
            // window bottom.
            Layout.minimumHeight: 0
            currentIndex: networkTab.checked ? 1 : 0

            GraphicsSettingsForm {
                config: root.config
                fontDatabase: root.fontDatabase
            }

            NetworkSettingsForm {
                network: root.network
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item {
                Layout.fillWidth: true
            }
            Button {
                text: "Cancel"
                onClicked: root.cancel()
            }
            Button {
                text: "Done"
                highlighted: true
                onClicked: root.done()
            }
        }
    }
}
