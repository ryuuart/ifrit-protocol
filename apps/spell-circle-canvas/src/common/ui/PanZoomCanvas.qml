pragma ComponentBehavior: Bound

import QtQuick

/**
 * Reusable infinite-style viewport for a fixed-size canvas item.
 *
 * Children declared inside the component are placed in the scaled canvas.
 * Mouse-wheel and pinch gestures zoom around the pointer; dragging and
 * touchpad scrolling pan without coupling this component to rendered content.
 */
Item {
    id: root

    required property real canvasWidth
    required property real canvasHeight
    default property alias canvasContent: contentContainer.data

    property real viewScale: fitScale
    property real horizontalPan: 0.0
    property real verticalPan: 0.0
    property real minimumScale: 0.04
    property real maximumScale: 16.0
    property bool checkerboardVisible: true
    /** Hides the built-in zoom and canvas-size badges for hosts that show
     *  that information in their own chrome (e.g. the app's activity
     *  panel), keeping the viewport itself unobstructed. */
    property bool showOverlays: true
    /** Width covered by floating chrome on the left; fitting and centering
     *  use the remaining region so a full-bleed viewport keeps its canvas
     *  in full view beside an overlaid panel. */
    property real leftContentInset: 0

    readonly property real fitScale: {
        const availableWidth = width - leftContentInset - 60;
        const availableHeight = height - 60;
        if (availableWidth <= 0 || availableHeight <= 0 || canvasWidth <= 0 || canvasHeight <= 0)
            return 1.0;
        return Math.min(availableWidth / canvasWidth, availableHeight / canvasHeight);
    }

    /** Fits the entire native canvas in the viewport and clears the pan. */
    function fitView() {
        viewScale = fitScale;
        horizontalPan = 0.0;
        verticalPan = 0.0;
    }

    /** Restores native pixel scale and clears the pan. */
    function zoomToActualSize() {
        viewScale = 1.0;
        horizontalPan = 0.0;
        verticalPan = 0.0;
    }

    /** Scales around a viewport-local pointer while keeping its content fixed. */
    function zoomAt(factor, pointerX, pointerY) {
        const newScale = Math.max(minimumScale, Math.min(maximumScale, viewScale * factor));
        const appliedFactor = newScale / viewScale;
        if (appliedFactor === 1.0)
            return;

        const horizontalDistance = pointerX - (leftContentInset + (width - leftContentInset) / 2) - horizontalPan;
        const verticalDistance = pointerY - height / 2 - verticalPan;
        viewScale = newScale;
        horizontalPan += horizontalDistance * (1.0 - appliedFactor);
        verticalPan += verticalDistance * (1.0 - appliedFactor);
    }

    clip: true

    Rectangle {
        anchors.fill: parent
        color: Theme.viewportBackground
    }

    Item {
        anchors.fill: parent
        opacity: 0.25

        Repeater {
            model: Math.max(0, Math.ceil(root.width / 32)) * Math.max(0, Math.ceil(root.height / 32))

            Rectangle {
                required property int index

                readonly property int columnCount: Math.max(1, Math.ceil(root.width / 32))
                readonly property int columnIndex: index % columnCount
                readonly property int rowIndex: Math.floor(index / columnCount)
                x: columnIndex * 32 + (root.horizontalPan % 32)
                y: rowIndex * 32 + (root.verticalPan % 32)
                width: 2
                height: 2
                color: Theme.secondaryText
            }
        }
    }

    Rectangle {
        x: scaledCanvas.x + 6
        y: scaledCanvas.y + 6
        width: scaledCanvas.width
        height: scaledCanvas.height
        color: "#000000"
        opacity: 0.55
        radius: 1
    }

    Checkerboard {
        x: scaledCanvas.x
        y: scaledCanvas.y
        width: scaledCanvas.width
        height: scaledCanvas.height
        paintWidth: root.width
        paintHeight: root.height
        visible: root.checkerboardVisible
    }

    Item {
        id: scaledCanvas
        width: root.canvasWidth * root.viewScale
        height: root.canvasHeight * root.viewScale
        x: root.leftContentInset + (root.width - root.leftContentInset - width) / 2 + root.horizontalPan
        y: (root.height - height) / 2 + root.verticalPan

        Item {
            id: contentContainer
            anchors.fill: parent
        }

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: Theme.border
            border.width: 1
        }
    }

    Rectangle {
        anchors {
            top: parent.top
            right: parent.right
            topMargin: 10
            rightMargin: 10
        }
        width: zoomBadgeText.width + 14
        height: 22
        radius: 3
        color: Theme.toolbarBackground
        opacity: 0.85
        visible: root.showOverlays

        Text {
            id: zoomBadgeText
            anchors.centerIn: parent
            text: Math.round(root.viewScale * 100) + "%"
            color: Theme.secondaryText
            font.pixelSize: 11
            font.family: Theme.monospaceFontFamily
        }
    }

    Rectangle {
        x: scaledCanvas.x + scaledCanvas.width - width - 2
        y: scaledCanvas.y + scaledCanvas.height + 6
        width: canvasSizeText.width + 12
        height: 18
        radius: 2
        color: Theme.toolbarBackground
        opacity: 0.7
        visible: root.showOverlays && y + height < root.height

        Text {
            id: canvasSizeText
            anchors.centerIn: parent
            text: root.canvasWidth + " × " + root.canvasHeight
            color: Theme.disabledText
            font.pixelSize: 10
            font.family: Theme.monospaceFontFamily
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.MiddleButton | Qt.LeftButton
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor

        property point previousPointerPosition

        onPressed: pointerEvent => {
            previousPointerPosition = Qt.point(pointerEvent.x, pointerEvent.y);
        }
        onPositionChanged: pointerEvent => {
            if (!pressed)
                return;
            root.horizontalPan += pointerEvent.x - previousPointerPosition.x;
            root.verticalPan += pointerEvent.y - previousPointerPosition.y;
            previousPointerPosition = Qt.point(pointerEvent.x, pointerEvent.y);
        }
    }

    WheelHandler {
        target: null
        acceptedDevices: PointerDevice.TouchPad
        onWheel: wheelEvent => {
            root.horizontalPan += wheelEvent.pixelDelta.x;
            root.verticalPan += wheelEvent.pixelDelta.y;
        }
    }

    WheelHandler {
        target: null
        acceptedDevices: PointerDevice.Mouse
        onWheel: wheelEvent => {
            const zoomFactor = Math.pow(1.4, wheelEvent.angleDelta.y / 120.0);
            root.zoomAt(zoomFactor, wheelEvent.x, wheelEvent.y);
        }
    }

    PinchHandler {
        target: null
        property real scaleAtStart: 1.0

        onActiveChanged: {
            if (active)
                scaleAtStart = root.viewScale;
        }
        onActiveScaleChanged: {
            if (!active)
                return;
            const zoomFactor = (scaleAtStart * activeScale) / root.viewScale;
            root.zoomAt(zoomFactor, centroid.position.x, centroid.position.y);
        }
    }
}
