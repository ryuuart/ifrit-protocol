---
kind: type
library: SigilScry
name: WebView
qualified: sigil::scry::WebView
group: The engine
status: stable
---

# WebView

One offscreen web page — a tab — rendered by the engine.

In threaded engines every method is safe to call from any thread:
commands (loads, input, script) are marshalled to the web thread, and
`WebView::frame` and `WebView::frameVersion` read the latest published
frame. In unthreaded engines everything runs inline on the caller's
thread. Holding a view keeps its engine alive.

## The frame is a snapshot, not a set of accessors

`WebView::Frame` is everything about the latest repaint in one value —
the acquire-latest-frame shape, rather than a separate accessor per
fact. A default-constructed one is falsy, and the bool conversion is
whether the view has painted anything yet.

`WebView::Frame::image` is the frame as a drawable image: always set on
CPU engines, and on GPU engines set when `WebView::frame` was given a
recorder to wrap the texture for. Wraps are cached per version, so the
image identity is stable across draws of the same frame and Skia-side
caches keyed on it stay warm.

`WebView::Frame::texture` is the published texture on GPU engines, named
on the engine's device, from which the native object can be exported. It
goes stale once the view republishes at a new size — the wrap in the
image is what keeps a frame's texture alive.

`WebView::Frame::dirtyBounds` is the page region that changed in this
repaint, in frame pixels, and the full bounds when unknown. A consumer
that blends the frame over live content uses it to limit its own repaint
area. `WebView::Frame::version` increases by one per repaint and is zero
before the first paint.

`WebView::frame` acquires that snapshot, falsy until the first repaint.
On GPU engines, pass the Graphite recorder you will draw with — over the
engine's device, on its shared context or another over the same queue —
to get the image populated with a zero-copy, per-version-cached wrap;
without a recorder you still get the texture and the metadata. On CPU
engines the recorder is ignored and the image is the raster frame. Call
it from the thread that owns the recorder.

`WebView::draw` draws the latest published frame scaled into a rect, and
is a no-op before the first repaint; on GPU engines the canvas must be
Graphite-backed, because its recorder wraps the frame texture.

`WebView::peekPixels` is zero-copy access to the LIVE surface pixels,
premultiplied BGRA. It is valid only on the web thread — from an
unthreaded engine's caller between render frames, or inside a frame
callback — and only until the next render frame or resize. It answers
false when unavailable.

## The two callbacks

`WebView::setFrameCallback` fires on the web thread after each repaint
publishes a new frame, and is what schedules a redraw of whatever
composites this view. The frame it carries has the metadata and, on CPU
engines, the raster image; a GPU consumer treats it as a signal and
acquires through `WebView::frame` on its own render thread.

`WebView::setRenderPassCallback` fires at the END of every pass the
engine makes over its pages — the pass that published a repaint of this
one and the pass that found nothing to publish alike — carrying how many
passes the engine has made. The count is the ENGINE's own tick, the same
number for every view over it, and a frame callback for the same pass
has already run when this one does.

It is what a page's STILLNESS is counted in: a stretch with no repaint
in it is a number of passes rather than a stretch of clock, so a machine
that runs the engine slowly and one that runs it fast call the same page
still on the same repaint.

`WebView::setLoadCallback` fires on the web thread when the main frame
finishes loading. `WebView::evaluateScript`'s own callback receives the
result, or the exception text, stringified, on that same thread.

## Input

`WebView::mouseMove`, `WebView::mouseDown` and `WebView::mouseUp` take
view pixels. The button on a move is the one held during it; on a press
or a release it is the one pressed or released, and the page sees no
click until the matching release arrives. `WebView::MouseButton::None`
is the only meaningful value for a move with nothing held.

`WebView::scroll` takes a wheel delta in pixels, exactly as an input
device delivers one. THE DELTA IS WHAT THE CONTENT MOVES BY, not where
the viewport goes — so WALKING DOWN A PAGE IS NEGATIVE: `scroll(0, -120)`
lifts the content 120 pixels and shows what stood below it, the way a
wheel rolled away from the reader does. Positive brings the page back
down toward its top, and the horizontal delta is the same statement
sideways, negative revealing what stood to the right.

## See also

- `engine/WebView.h` — the header: `WebView`
- [WebEngine](WebEngine.md) — what creates and owns a view
- [WebImage](WebImage.md) — the other direction, drawing INTO a page
