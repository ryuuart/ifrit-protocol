import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ifrit.Ui 1.0 as Ui
import TextFlow.Gallery

/** Top-level composition for the interactive TextFlow gallery. */
ApplicationWindow {
    id: window

    required property int initialScene
    required property string initialText

    width: 1360
    height: 860
    visible: true
    title: "TextFlow Gallery — " + galleryView.sceneNames[galleryView.sceneIndex]
    color: Ui.Theme.windowBackground

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        GallerySidebar {
            Layout.preferredWidth: 320
            Layout.minimumWidth: 320
            Layout.maximumWidth: 320
            Layout.fillHeight: true
            view: galleryView
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 6
            color: Ui.Theme.canvasBackground
            clip: true

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
