# The values

What you CREATE to put a web page into a drawing, and what a page hands
back. Three objects and nothing else: one engine, the pages over it, and
the slots those pages display.

Every value page answers the same three questions in the same order:
**Make one** — every spelling that produces the value; **Pass it to** —
every slot that takes it; and **Also returned by** — what hands one
back.

## The engine

| Value | What it is | Header |
|---|---|---|
| [`WebEngine`](pages/types/WebEngine.md) | The Ultralight renderer and the thread that drives it, with `WebEngineConfig` fixing what bring-up builds and `ViewOptions` what a view is made with. | `engine/WebEngine.h` |
| [`WebView`](pages/types/WebView.md) | One offscreen page, its input, its script, and the frames it publishes — each repaint an immutable `WebView::Frame` with a version. | `engine/WebView.h` |
| [`WebImage`](pages/types/WebImage.md) | A named slot a page displays and native code fills, painted through a canvas, copied from raster pixels, or blitted from a texture. | `engine/WebImage.h` |

`README.md` beside this file is the library: the mental model, the three
features and the one seam each, and the conventions that will bite you.

## The headers these come from

- `engine/WebEngine.h` — `WebEngine`, `WebEngineConfig`, `ViewOptions`
- `engine/WebView.h` — `WebView`
- `engine/WebImage.h` — `WebImage`
- `platform/LogLevel.h` — `LogLevel`
- `platform/Runtime.h` — `available`
