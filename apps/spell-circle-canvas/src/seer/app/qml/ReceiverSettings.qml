import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Qt 1.0 as Ui
import SpellCircle.Canvas 1.0

Ui.AppWindow {
    id: settings
    required property var receiver
    property string error: ""
    property bool accepted: false
    title: "Scene Receiver Settings"
    flags: Qt.platform.os === "osx" ? Qt.Sheet : Qt.Dialog
    width: 500
    height: 640
    minimumWidth: 480
    minimumHeight: 540
    color: Ui.Theme.windowBackground

    function edit() {
        if (visible) {
            requestActivate();
            return;
        }
        receiver.beginSettings();
        accepted = false;
        error = "";
        show();
        requestActivate();
    }
    onClosing: {
        if (!accepted) receiver.cancelSettings();
    }
    Shortcut { sequences: [StandardKey.Cancel]; onActivated: settings.close() }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        Ui.SectionHeading { text: "Scene appearance" }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            GraphicsSettingsForm {
                width: parent.width
                config: settings.receiver.config
                fontDatabase: Ui.FontDatabase
            }
        }
        Label {
            Layout.fillWidth: true
            text: settings.error
            color: Ui.Theme.errorText
            visible: text.length > 0
            wrapMode: Text.Wrap
        }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button { text: "Cancel"; onClicked: settings.close() }
            Button {
                text: "Done"
                highlighted: true
                onClicked: {
                    if (settings.receiver.saveSettings()) {
                        settings.accepted = true;
                        settings.close();
                    } else settings.error = "The receiver settings could not be saved.";
                }
            }
        }
    }
}
