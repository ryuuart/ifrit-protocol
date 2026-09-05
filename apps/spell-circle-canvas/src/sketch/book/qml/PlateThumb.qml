// A SKETCH AS ONE PICTURE: the still the quick tier photographed it as.
//
// A sketch with no plate gets a drawn glyph for the runtime it draws
// through rather than a blank — the two runtimes are the one thing about
// an unphotographed sketch worth seeing from across a list.

import QtQuick
import Sigil.Sketchbook

Rectangle {
    id: thumb

    /** The plate file, or empty where the store has none for this sketch
     *  yet — in which case one is asked for as the thumbnail comes on
     *  screen. */
    property url plate
    /** "canvas", "set" or "draw" — which glyph stands in for a missing
     *  plate. */
    property string kind
    /** The catalog to ask for a render, and the sketch this thumbnail is
     *  of. A missing or stale thumbnail is requested as the row appears
     *  and abandoned as it scrolls away, so the one background render is
     *  spent on what is on screen. */
    property var catalog: null
    property int sketchIndex: -1
    /** How many pixels wide the decoder is asked for. One number for a
     *  row and a card is deliberate: the same scaled image then serves
     *  both out of Qt's cache instead of being decoded twice. */
    property int decodeWidth: 320

    onKindChanged: glyph.requestPaint()

    // Ask for a still as this thumbnail comes on screen, and give up the
    // request as it leaves — the worker renders one at a time, in the
    // order rows asked, so keeping the queue to what is visible is what
    // makes it feel immediate.
    function askForThumbnail() {
        if (thumb.catalog && thumb.sketchIndex >= 0
            && String(thumb.plate).length === 0)
            thumb.catalog.requestThumbnail(thumb.sketchIndex);
    }
    Component.onCompleted: thumb.askForThumbnail()
    Component.onDestruction: {
        if (thumb.catalog && thumb.sketchIndex >= 0)
            thumb.catalog.cancelThumbnail(thumb.sketchIndex);
    }
    onSketchIndexChanged: thumb.askForThumbnail()
    onPlateChanged: thumb.askForThumbnail()

    color: Theme.ground
    radius: 4
    clip: true

    Image {
        id: image

        anchors.fill: parent
        source: thumb.plate
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectCrop
        sourceSize.width: thumb.decodeWidth
        visible: status === Image.Ready
    }

    Canvas {
        id: glyph

        anchors.centerIn: parent
        width: Math.min(thumb.width, thumb.height) * 0.42
        height: glyph.width
        visible: !image.visible
        opacity: 0.5
        onWidthChanged: glyph.requestPaint()

        onPaint: {
            const context = glyph.getContext("2d");
            context.reset();
            context.strokeStyle = Theme.faint;
            context.lineWidth = Math.max(1, glyph.width * 0.06);
            const w = glyph.width;
            const h = glyph.height;
            if (thumb.kind === "set") {
                // A box seen from a corner: the silhouette, then the
                // three edges that meet at the near vertex — the least
                // that reads as a solid rather than as a hexagon.
                context.beginPath();
                context.moveTo(w * 0.5, h * 0.04);
                context.lineTo(w * 0.96, h * 0.28);
                context.lineTo(w * 0.96, h * 0.72);
                context.lineTo(w * 0.5, h * 0.96);
                context.lineTo(w * 0.04, h * 0.72);
                context.lineTo(w * 0.04, h * 0.28);
                context.closePath();
                context.stroke();
                context.beginPath();
                context.moveTo(w * 0.5, h * 0.52);
                context.lineTo(w * 0.5, h * 0.96);
                context.moveTo(w * 0.5, h * 0.52);
                context.lineTo(w * 0.04, h * 0.28);
                context.moveTo(w * 0.5, h * 0.52);
                context.lineTo(w * 0.96, h * 0.28);
                context.stroke();
            } else {
                // A frame with a mark inside it: a surface something was
                // drawn onto.
                context.strokeRect(w * 0.06, h * 0.14, w * 0.88, h * 0.72);
                context.beginPath();
                context.arc(w * 0.5, h * 0.5, w * 0.2, 0, Math.PI * 2);
                context.stroke();
            }
        }
    }
}
