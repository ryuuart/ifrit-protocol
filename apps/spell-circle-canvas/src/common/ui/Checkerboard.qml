import QtQuick

/**
 * Screen-space transparency checkerboard clipped to arbitrary content bounds.
 *
 * paintWidth and paintHeight describe the containing viewport, so zooming the
 * clipped content never scales the tiles or allocates a huge drawing surface.
 */
Item {
    id: root

    required property real paintWidth
    required property real paintHeight
    property real tileSize: 10
    property color lightColor: "#cccccc"
    property color darkColor: "#999999"

    clip: true

    Canvas {
        id: checkerboardCanvas
        x: -root.x
        y: -root.y
        width: root.paintWidth
        height: root.paintHeight

        onPaint: {
            const context = getContext("2d");
            context.fillStyle = root.lightColor;
            context.fillRect(0, 0, width, height);
            context.fillStyle = root.darkColor;

            const columnCount = Math.ceil(width / root.tileSize);
            const rowCount = Math.ceil(height / root.tileSize);
            for (let rowIndex = 0; rowIndex < rowCount; ++rowIndex) {
                for (let columnIndex = rowIndex % 2; columnIndex < columnCount; columnIndex += 2)
                    context.fillRect(columnIndex * root.tileSize, rowIndex * root.tileSize, root.tileSize, root.tileSize);
            }
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    onTileSizeChanged: checkerboardCanvas.requestPaint()
    onLightColorChanged: checkerboardCanvas.requestPaint()
    onDarkColorChanged: checkerboardCanvas.requestPaint()
}
