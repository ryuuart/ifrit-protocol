# Ifrit.Ui

A small set of reusable Qt Quick components shared by the desktop tools in
this repository, plus one C++ helper for native window dressing. It is a QML
module named `Ifrit.Ui`: a pan-and-zoom viewport for fixed-size canvases, a
transparency checkerboard, font pickers, a rounded-corner panel, a theme
singleton derived from the system palette, and a singleton that installs
macOS window vibrancy.

Nothing here is application-specific. Components take their data as
injected properties and functions and report changes with signals, so they
work against a C++ model, a QML model, or a hard-coded list equally well.

## Using it

Import the module and use the types:

```qml
import QtQuick
import QtQuick.Controls
import Ifrit.Ui 1.0 as Ui

ApplicationWindow {
    id: window
    color: Ui.Theme.windowBackground

    Component.onCompleted: {
        if (Ui.WindowChrome.applyVibrancy(window))
            window.color = "transparent";
        Ui.WindowChrome.setSubtitle(window, "document.txt");
    }

    Ui.PanZoomCanvas {
        anchors.fill: parent
        canvasWidth: 1920
        canvasHeight: 1080

        MyRenderedItem { anchors.fill: parent }
    }
}
```

Children declared inside `PanZoomCanvas` are placed in the scaled canvas;
the component handles wheel and pinch zoom around the pointer, drag and
touchpad panning, and exposes `fitView()`, `zoomToActualSize()` and
`zoomAt(factor, pointerX, pointerY)` plus the `viewScale`, `horizontalPan`
and `verticalPan` state behind them.

The font controls follow the injection rule. `FontFamilyField` takes a
`searchFamilies(query)` function and emits `familyChosen`; `FontSelector`
wraps family, style and size, taking a `fontDatabase` object that provides
`searchFamilies()`, `styles()`, `font()` and `styleForFont()`, and emitting
`fontModified(fontValue)` rather than writing back into the value it was
given:

```qml
Ui.FontSelector {
    fontDatabase: appFontCatalog
    selectedFont: config.bodyFont
    onFontModified: value => config.bodyFont = value
}
```

`DimensionSpinBoxes` is the same shape for a width-by-height pair
(`widthValue` / `heightValue` in, `widthModified` / `heightModified` out).

## The mental model

**The theme is the system palette.** `Theme` is a QML singleton that derives
every chrome colour from `SystemPalette`, so windows follow the OS light and
dark appearance with no per-app switch. Surface colours carry alpha so they
read as tinted glass over a vibrant window and still degrade to sensible
solids on an opaque one. `Theme.darkMode` is derived, not configured.

**Vibrancy is opt-in and may decline.** `WindowChrome.applyVibrancy()`
returns whether the window is now vibrant and should therefore be made
transparent. Where it returns false, the caller keeps its opaque
palette-driven background — which is why the pattern above is an `if` and
not a bare call.

**Components inject, they do not depend.** No component here reaches for an
application model, a font database, or a document type. That is what makes
the same viewport usable by a text-layout gallery and a scene canvas.

## Gotchas

**Both `WindowChrome` methods return false off Apple**, and `setSubtitle`
also returns false below macOS 11. Callers must have a fallback path — an
opaque background, and the scene name folded into the composite window
title. Treat the return value as the branch, not as a diagnostic.

**`applyVibrancy` is idempotent.** It recognizes the effect view it
previously inserted, so calling it again on the same window is harmless.
Setting `IFRIT_NO_VIBRANCY` in the environment makes it decline outright,
which is the escape hatch when debugging compositing.

**The vibrancy view goes behind Qt's content view, not inside it.** Qt's
view *is* the window's content view and subviews always draw above their
superview, so the effect view is inserted into the frame view below it. This
is why the window must also be made transparent for the glass to show.

**`Checkerboard` is screen-space on purpose.** It takes `paintWidth` and
`paintHeight` describing the containing viewport, and positions its canvas
against that rather than against its own bounds. Zooming the clipped content
therefore never scales the tiles or allocates a viewport-sized-times-zoom
drawing surface. Pass the viewport's dimensions, not the content's.

**`GlassPanel` masks with a `MultiEffect` layer** rather than clipping,
because a plain rectangular `clip` cannot round off live GPU content. Its
children go through a layer, so it is not free — use it for panels, not for
every rounded rectangle.

**`PanZoomCanvas` has two insets you may need.** `leftContentInset` tells it
how much width is covered by floating chrome, so fitting and centring use
the remaining region; `showOverlays: false` hides the built-in zoom and
canvas-size badges for hosts that display that information in their own UI.

## Boundary

Public dependencies: `Qt6::Quick`, `Qt6::QuickControls2`,
`Qt6::QuickLayouts`. On Apple, AppKit privately — elsewhere a stub
implementation stands in. Nothing else under `common/` is involved, and this
module knows nothing about Skia, scene content, or the products that use it.

## Building

One target, `IfritUi`, always configured; it is a static Qt library declared
with `qt_add_qml_module(URI Ifrit.Ui VERSION 1.0)`. There are no tests and
no assets.

QML types provided: the `Theme` singleton, `Checkerboard`, `PanZoomCanvas`,
`GlassPanel`, `DimensionSpinBoxes`, `FontFamilyField`, `FontSelector`. The
C++ side registers `WindowChrome` as a QML singleton with two invokable
methods, `applyVibrancy(QQuickWindow *)` and
`setSubtitle(QQuickWindow *, const QString &)`.
