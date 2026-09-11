pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Ifrit.Ui 1.0 as Ui
import Sigil.Sketchbook

ColumnLayout {
    id: pane
    spacing: 0

    property alias sketchIndex: view.sketchIndex
    property alias paused: view.paused
    property alias timeScale: view.timeScale
    readonly property var metrics: view.metrics
    readonly property string hostState: view.state
    readonly property string status: view.status
    readonly property bool orbitable: view.orbitable
    readonly property bool canvasFocused: view.activeFocus
    signal captureReady(string path)
    signal thumbnailCaptured(int index)

    function capture() { view.capture(); }

    Ui.PanZoomCanvas {
        id: canvasViewport

        Layout.fillWidth: true
        Layout.fillHeight: true
        canvasWidth: {
            const dimensions = (view.metrics.canvas ?? "")
                .split("x");
            const value = Number(dimensions[0]);
            return dimensions.length === 2 && value > 0
                ? value : 900;
        }
        canvasHeight: {
            const dimensions = (view.metrics.canvas ?? "")
                .split("x");
            const value = Number(dimensions[1]);
            return dimensions.length === 2 && value > 0
                ? value : 640;
        }
        checkerboardVisible: false
        // The largest registry canvases still fit inside the
        // common 16K texture limit at 4× on a Retina screen.
        maximumScale: 4.0
        panButtons: Qt.MiddleButton
        showPanCursor: false
        // A set sketch owns the ordinary wheel for camera
        // distance. Ctrl-wheel still reaches zoom below.
        mouseWheelZoomEnabled: !view.orbitable

        SketchbookView {
            id: view

            anchors.fill: parent
            // Captures run on the render thread; the saved path
            // (or an empty string on failure) arrives
            // asynchronously.
            onCaptureReady: path => pane.captureReady(path)
            onThumbnailCaptured: index => pane.thumbnailCaptured(index)

            // Orbit, for the sketches that have a viewpoint to
            // move. A drag is yaw and pitch; the wheel is
            // distance. A sketch with no viewpoint gets no
            // handler at all, so a drag over a drawn tree does
            // nothing rather than something invisible.
            //
            // EVERY GESTURE STARTS FROM WHERE THE SKETCH STANDS,
            // read off the view at the moment it begins, so an
            // untouched sketch is seen from the camera it
            // declared and the first drag moves that camera
            // rather than replacing it.
            property real yaw: 0
            property real pitch: 0
            property real distance: 0

            DragHandler {
                enabled: view.orbitable
                target: null
                property real startYaw: 0
                property real startPitch: 0
                onActiveChanged: {
                    if (active) {
                        startYaw = view.orbitYaw;
                        startPitch = view.orbitPitch;
                        view.distance = view.orbitDistance;
                    }
                }
                onTranslationChanged: {
                    view.yaw = startYaw - translation.x * 0.4;
                    view.pitch = startPitch + translation.y * 0.3;
                    view.orbit(view.yaw, view.pitch, view.distance);
                }
            }
            WheelHandler {
                enabled: view.orbitable
                onWheel: event => {
                    if (event.modifiers & Qt.ControlModifier) {
                        const point = view.mapToItem(
                            canvasViewport, event.x, event.y);
                        const factor = Math.pow(
                            1.4, event.angleDelta.y / 120.0);
                        canvasViewport.zoomAt(
                            factor, point.x, point.y);
                        return;
                    }
                    view.yaw = view.orbitYaw;
                    view.pitch = view.orbitPitch;
                    view.distance = Math.max(
                        40,
                        view.orbitDistance
                            - event.angleDelta.y * 0.5);
                    view.orbit(
                        view.yaw, view.pitch, view.distance);
                }
            }

            // The pointer and the keys, for the sketches that
            // read them. Neither handler takes the grab, so the
            // orbit above still drags a set; a sketch with
            // nothing for a pointer to do ignores what arrives.
            // The hover reports where the pointer stands with
            // no button down, the point handler while one is,
            // and a click gives this canvas the keyboard — so
            // the keys go to the sketch after it is clicked and
            // back to the list when the list is.
            HoverHandler {
                id: hover
                onPointChanged: {
                    if (!press.active)
                        view.pointer(point.position.x,
                                     point.position.y, false);
                }
            }
            PointHandler {
                id: press
                acceptedButtons: Qt.LeftButton
                onActiveChanged: view.pointer(point.position.x,
                                              point.position.y,
                                              active)
                onPointChanged: {
                    if (active)
                        view.pointer(point.position.x,
                                     point.position.y, true);
                }
            }
            TapHandler {
                onTapped: view.forceActiveFocus()
            }
            Keys.onPressed: event => {
                view.key(event.key, event.text, true);
                event.accepted = true;
            }
            Keys.onReleased: event => {
                view.key(event.key, event.text, false);
                event.accepted = true;
            }
        }
    }

    // Compile-error overlay: the last good sketch keeps
    // running underneath.
    Rectangle {
        Layout.fillWidth: true
        visible: view.errorLog.length > 0
        color: "#2a0c12"
        Layout.preferredHeight: Math.min(
            errorText.implicitHeight + 20, pane.height * 0.4)
        ScrollView {
            id: errorScroll

            anchors.fill: parent
            anchors.margins: 10
            Text {
                id: errorText

                text: view.errorLog
                color: Theme.bad
                font.family: Theme.mono
                font.pixelSize: 12
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
                width: errorScroll.width - 20
            }
        }
    }
}
