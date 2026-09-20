---
kind: type
library: SigilScry
name: WebImage
qualified: sigil::scry::WebImage
group: The engine
status: stable
---

# WebImage

Skia content composited INTO web pages — the reverse direction of a
view's output. `WebEngine::createImage` registers one under a name, and
a page references it wherever an image URL is accepted:

```html
<img src="name.imgsrc" />
```

The engine's own file system synthesizes the `.imgsrc` indirection
Ultralight expects: any path whose filename is `<name>.imgsrc` resolves
to the image registered under that name. Holding an image keeps its
engine alive, and destroying it unregisters the name.

## Four ways to supply pixels, safest first

**`WebImage::paint`** hands you a canvas already targeting the image's
pixels — a Graphite surface on the web thread's own recorder on GPU
engines, the shared bitmap on CPU ones — and handles the flush and the
invalidate in the same step, so a partial update cannot be observed and
no step can be forgotten. It is mode-agnostic, and raster images drawn
there upload through the recorder's image provider like anywhere else.
It is safe from any thread and the callback runs on the web thread,
blocking the caller until it is done, so nothing inside it should call
another engine door that posts and waits. The canvas is not cleared
first, and it answers false if the backend wrap failed.

**`WebImage::update`** copies raster pixels in, converting to
premultiplied BGRA, and invalidates. It works on CPU and GPU engines
alike. The image overload copies a raster-backed image on any engine;
a texture-backed one is recorder-bound and cannot be read from there,
so it logs a warning and answers false.

**`WebImage::updateTexture`** is for GPU engines: it blit-copies a
texture named on the engine's device — one another renderer produced,
say — into the slot and invalidates, on the web thread. The copy is
clamped to the smaller of the two sizes, and the texture must stay alive
until the call returns. It answers false on a CPU engine and for a stale
handle.

**The expert path** on a GPU engine is to render straight into the
texture behind `WebImage::texture` with your own Graphite recorder over
the engine's device, submit that work, and then call
`WebImage::invalidate` — in that order. The texture is named on the
engine's device and is valid for the image's lifetime, exporting its
native object on demand, and is null on CPU engines.

`WebImage::invalidate` notifies pages displaying this image that it
changed and should be redrawn; the update doors do it themselves.

## See also

- `engine/WebImage.h` — the header: `WebImage`
- [WebEngine](WebEngine.md) — what registers a slot
- [WebView](WebView.md) — the other direction, a page's output
