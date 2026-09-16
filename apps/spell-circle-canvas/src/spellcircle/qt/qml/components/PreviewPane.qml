import QtQuick
import Ifrit.Qt 1.0 as Ui
import SpellCircle.Canvas 1.0

/** The scene viewport, with fit and actual-size actions over shared pan/zoom. */
Item {
    id: root

    required property var model
    required property var config
    readonly property real viewScale: canvasViewport.viewScale

    function fitView() {
        canvasViewport.fitView();
    }

    function zoomToActualSize() {
        canvasViewport.zoomToActualSize();
    }

    Ui.PanZoomCanvas {
        id: canvasViewport
        anchors.fill: parent
        canvasWidth: root.config.canvas.width
        canvasHeight: root.config.canvas.height
        // Zoom and canvas size are shown in the receiver controls.
        showOverlays: false

        SpellCircle {
            anchors.fill: parent
            clip: true
            alphaBlending: true
            fillColor: "transparent"
            model: root.model
            config: root.config
        }
    }
}
