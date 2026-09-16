# Ifrit.Qt

A set of reusable Qt components and services shared by the desktop tools in
this repository, including SigilWeave's gallery. It is a QML
module named `Ifrit.Qt`: shared visual tokens, panels, headings, search,
selection, status and value controls, a pan-and-zoom viewport for fixed-size
canvases, a transparency checkerboard, font pickers, and a singleton that
installs macOS window vibrancy and supports continuous background rendering.
The same library owns the shared font database used by searchable pickers.
Its C++ window capture helper also supports repeatable screenshots of live
Qt Quick applications.

Nothing here is application-specific. Components take their data as
injected properties and functions and report changes with signals, so they
work against a C++ model, a QML model, or a hard-coded list equally well.

## Using it

Import the module and use the types:

```qml
import QtQuick
import QtQuick.Controls
import Ifrit.Qt 1.0 as Ui

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
    fontDatabase: Ui.FontDatabase
    selectedFont: config.bodyFont
    onFontModified: value => config.bodyFont = value
}
```

Font-family results support Up and Down while the search field keeps focus;
Enter chooses the selected result and Escape dismisses the results. Editing
the query clears the previous selection.

`DimensionSpinBoxes` is the same shape for a width-by-height pair
(`widthValue` / `heightValue` in, `widthModified` / `heightModified` out).

`FontDatabase` is a QML singleton over Qt's installed font families and
styles. Its `families()`, `searchFamilies(query)`, `styles(family)`,
`font(family, style, pointSize)` and `styleForFont(font)` methods supply the
shared pickers; an application may still inject a different catalogue.

## Shared application controls

`Panel` is a padded container with a border and rounded background. Its
children live in the content area, so a layout uses `anchors.fill: parent`
without repeating the padding. It does not mask or allocate a layer.
Use `GlassPanel` when GPU content itself must be clipped to rounded corners.

`SectionHeading` gives section labels one type style. `FactRow` takes
`label` and `value`, with optional `labelWidth`, `valueColor`, `valueElide`
and `monospace`. A truncated value is available in its tooltip.

`IconButton` retains the standard ToolButton action, checked state, keyboard
focus and click signal. Its `tooltip` is also its accessible name.
`SearchField` retains the standard TextField API, provides a clear button,
clears on Escape and emits `steppedOut` on Down for a host to focus results.
Both typing and clearing emit `textEdited`.

`SegmentedControl` takes an array of `{text, enabled}` records and a
`currentIndex`. It emits `activated(index)` only for a user's choice;
assigning an index from a model does not write back. The host handles that
signal and supplies the new index. A missing `enabled` field means enabled.

`SliderField` takes `label`, `value`, `from`, `to`, `stepSize`, `decimals`
and `suffix`. Only an interaction emits `valueEdited(value)`. A caller may
set `resetEnabled` and `resetValue`, then handle `resetRequested`.

`StatusIndicator` takes `text`, a semantic `tone` (`neutral`, `good`,
`warning`, `error`) and `busy`. Text names the state as well as its colour;
the component does not interpret a host's connection or rendering states.

## The mental model

**The theme is the system palette.** `Theme` is a QML singleton that derives
every chrome colour from `SystemPalette`, so windows follow the OS light and
dark appearance with no per-app switch. Surface colours carry alpha so they
read as tinted glass over a vibrant window and still degrade to sensible
solids on an opaque one. `Theme.darkMode` is derived, not configured.
Semantic success, warning and error colours have light and dark variants.
The shared body, caption and heading sizes, spacing, corner radii and
control heights keep application chrome consistent. Scene content retains
its own artistic palette.

**Font fallback resolves once in the theme.** `Theme.monospaceFontFamily`
selects the first installed family from its monospace preferences, or the
application font when none is installed. Controls pass that single name to
`font.family`.

**Vibrancy is opt-in and may decline.** `WindowChrome.applyVibrancy()`
returns whether the window is now vibrant and should therefore be made
transparent. Where it returns false, the caller keeps its opaque
palette-driven background — which is why the pattern above is an `if` and
not a bare call.

**Components inject, they do not depend.** No component here reaches for an
application model, a font database, or a document type. That is what makes
the same viewport usable by a text-layout gallery and a scene canvas.

## Gotchas

**Vibrancy and subtitles return false off Apple**, and `setSubtitle`
also returns false below macOS 11. Callers must have a fallback path — an
opaque background, and the scene name folded into the composite window
title. Treat the return value as the branch, not as a diagnostic.

**Background rendering is explicit.** C++ hosts call
`WindowChrome::keepRendering` on the GUI thread when frame publication must
continue behind another window. It enables Qt graphics and scene-graph
persistence. On macOS it also keeps an activity token until application exit
and opts that native window into rendering while covered. Offscreen windows
never enter the Cocoa path. This does not change window ordering or make a
minimized window visible.

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

## Window capture

`ifrit::qt::captureWindow` drives a window on its GUI thread, writes a PNG,
prints its path and dimensions, and exits the application. Each timer tick
grabs a frame so covered windows still render. `ifrit::qt::WindowCaptureOptions`
sets the warmup frame count and timer interval; optional `prepareFrame` and
`ready` callbacks let a host request scene updates and wait for content.
Readiness waiting is bounded by `maxReadyFrames`, after which warmup and
capture continue so loading and error states can also be inspected. Destroying
the window cancels the capture. A failed grab or save exits with status 1.

## Boundary

Public dependencies: `Qt6::Quick`, `Qt6::QuickControls2`,
`Qt6::QuickLayouts`. On Apple, AppKit privately — elsewhere a stub
implementation stands in. Nothing else under `common/` is involved, and this
module knows nothing about Skia, scene content, or the products that use it.

## Building

One target, `IfritQt`, always configured; it is a static Qt library declared
with `qt_add_qml_module(URI Ifrit.Qt VERSION 1.0)`. The `qt_qml_test` target
checks keyboard activation, font-result navigation, search clearing, disabled
choices and model updates that must not emit user-edit signals. There are no
assets.

QML types provided: the `Theme` singleton, `Panel`, `SectionHeading`,
`FactRow`, `StatusIndicator`, `IconButton`, `SegmentedControl`, `SearchField`,
`SliderField`, `FontDatabase`, `Checkerboard`, `PanZoomCanvas`, `GlassPanel`,
`DimensionSpinBoxes`, `FontFamilyField`, `FontSelector`. The
C++ side registers `WindowChrome` as a QML singleton with two invokable
methods, `applyVibrancy(QQuickWindow *)` and
`setSubtitle(QQuickWindow *, const QString &)`.
