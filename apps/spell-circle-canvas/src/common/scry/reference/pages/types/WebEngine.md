---
kind: type
library: SigilScry
name: WebEngine
qualified: sigil::scry::WebEngine
group: The engine
status: stable
---

# WebEngine

Owns the Ultralight web renderer and the thread that drives it.

Ultralight renders HTML, CSS and JavaScript documents — full WebKit
layout: flexbox, grid, custom fonts, SVG, canvas, animations — into
offscreen buffers. This engine wraps it so each `WebView`'s output is a
premultiplied-BGRA image, ready to draw onto any canvas, raster or
GPU-backed.

## Three ways to integrate, least to most coupled

1. **Pull.** Draw `WebView::frame`'s image whenever you repaint, using
   `WebView::frameVersion` to skip work when nothing changed.
2. **Push.** `WebView::setFrameCallback` fires on the web thread each
   time the page repaints — use it to schedule a redraw of your canvas.
3. **Lockstep.** Create the engine unthreaded and call `WebEngine::update`
   and `WebEngine::renderFrame` from your own render loop. `WebView::draw`
   then composites the freshly rendered surface in the same frame, and
   `WebView::peekPixels` exposes the live surface with zero copies.

`WebEngine::update` dispatches Ultralight's timers, callbacks and
network events, and wants calling at least once per frame and ideally
more often. `WebEngine::renderFrame` repaints dirty views and publishes
their frames, answering whether any view actually repainted. Both are
unthreaded mode only.

## One renderer per process, and it is never released

Ultralight allows exactly one renderer per process and its teardown
cannot be run, so the runtime the first `WebEngine::create` boots — the
renderer, the platform handlers and the web thread — is the PROCESS's
and is never released.

Releasing an engine ends that engine: its views go and its runtime
parks, holding everything, and the next create stands the same runtime
up again. Only ONE engine at a time. `WebEngine::create` answers null
while another is still held, when a configuration names something
bring-up fixed for the process — the resource roots, the session store,
the threading, the device — and when bring-up itself fails.

Views keep the engine alive, so destruction order between a `WebView`
handle and a `WebEngine` handle is free.

## Configuring it

`WebEngineConfig` is fixed at create time, and the defaults run out of
the box: resources — the ICU tables and the CA certificates — come from
the directory baked in at build time, and log output goes to standard
error for warnings and errors.

| Field | What it says |
| --- | --- |
| `WebEngineConfig::resourceDirectory` | Where `icudt67l.dat` and `cacert.pem` are. Empty uses the `resources` folder next to the executable, falling back to the SDK install location found at configure time. |
| `WebEngineConfig::fileSystemDirectory` | The base directory `file:///` URLs resolve against. |
| `WebEngineConfig::cachePath` | A writable directory for persistent session data — cookies, local storage. Empty keeps everything in memory. |
| `WebEngineConfig::deviceScale` | Page units to pixels for new views; 2.0 for HiDPI output. `ViewOptions::deviceScale` overrides it per view when above zero. |
| `WebEngineConfig::framesPerSecond` | The render thread's target cadence, ignored when unthreaded. |
| `WebEngineConfig::threaded` | True gives the engine a dedicated web thread that pumps Ultralight and publishes frames, and every view call is then safe from any thread. False hands the loop to the caller: create, every view call, update and renderFrame must all happen on ONE thread. |
| `WebEngineConfig::logCallback` | Receives Ultralight's log and console output plus engine diagnostics, from engine-internal threads — usually the web thread. |
| `WebEngineConfig::gpuDevice` | The device the host draws with, adopted or owned, kept alive by the host for the engine's lifetime. |
| `WebEngineConfig::graphite` | The Graphite context the engine's own drawing shares with the host. |

`ViewOptions::transparent` gives a view a transparent background; pair
it with `html,body{background: transparent}` in the page's own CSS.

## The two backends behind one API

With a device set, views render through Ultralight's GPU pipeline into
textures named by that device's handles and publish texture-backed
frames, which `WebView::frame` wraps zero-copy for a Graphite recorder.
With none, the CPU renderer publishes raster images instead. Bring-up
that fails falls back to CPU with a logged warning, as does a platform
with no driver yet. The engine internals are backend-neutral, so a
Vulkan driver joins without touching the rest of the library.

`WebImage::paint` records on the web thread's own recorder over the
shared Graphite context and submits under its lock, so a host that
shares the context and uses it from its own thread makes every context
call under that same lock. Leaving the context null makes the engine
create one of its own over the device, which stays correct — one queue
orders both — and costs nothing but a second context. It is ignored
without a device.

## See also

- `engine/WebEngine.h` — the header: `WebEngine`, `WebEngineConfig`,
  `ViewOptions`
- [WebView](WebView.md) — one offscreen page over this engine
- [WebImage](WebImage.md) — the slot a page displays, filled from here
