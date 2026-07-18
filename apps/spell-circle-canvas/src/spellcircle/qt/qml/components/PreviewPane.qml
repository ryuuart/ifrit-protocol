import QtQuick
import Ifrit.Ui 1.0 as Ui
import SpellCircle.Canvas 1.0

/**
 * The canvas viewport, rendered full-bleed behind the floating chrome. The
 * fit view keeps the canvas in full view right of the activity panel (via
 * leftContentInset), so nothing obstructs it. Zoom state and view actions
 * are exposed for the activity panel's readouts and buttons, mirroring the
 * native macOS app.
 */
Item {
    id: root

    required property var model
    required property var config
    /** Width covered by the floating activity panel, handed through to the
     *  viewport's fit/centering math. */
    property real leftContentInset: 0
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
        leftContentInset: root.leftContentInset
        // Zoom and canvas size are shown in the activity panel instead;
        // the viewport itself stays unobstructed.
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
