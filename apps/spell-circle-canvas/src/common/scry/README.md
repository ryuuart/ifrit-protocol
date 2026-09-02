# SigilScry

SigilScry embeds a headless web browser in a C++ application and hands you
its output as Skia images. It wraps the [Ultralight](https://ultralig.ht)
SDK — a WebKit-derived engine that lays out HTML, CSS and JavaScript
offscreen — so a page can be drawn onto any `SkCanvas` like an ordinary
image. Compositing works in both directions: native Skia drawing can also
be placed *inside* a page through named image slots.

Namespace `sigil::scry`. One feature library per directory; every public
header lives under `include/sigilscry/<feature>/` and is spelled
`<sigilscry/<feature>/X.h>`:

| target | headers | holds |
|--------|---------|-------|
| `SigilScryPlatform` | `platform/LogLevel.h`, `platform/Runtime.h` | `LogLevel`, the severity every engine message carries; `runtime::available(why)`, whether the resource directory an engine would boot with holds the runtime data it needs. The rest of the feature — the `SkBitmap`-backed surface, the two-root file system, the logger bridge, the resource directory — is what Ultralight's `Platform` singleton is handed, and stays internal |
| `SigilScryGpu`      | — | Ultralight's GPU command lists executed on a SigilSkia `GpuDevice`, and the texture interop the engine needs beyond that; the graphics-API-neutral contract and its Metal implementation are internal |
| `SigilScryEngine`   | `engine/WebEngine.h`, `engine/WebView.h`, `engine/WebImage.h` | `WebEngineConfig`, `ViewOptions` and `WebEngine`; `WebView` and its `Frame`; `WebImage` |

`SigilScry` is the umbrella target over all three, and
`<sigilscry/Scry.h>` the umbrella header. A consumer links `SigilScry`
and includes the engine headers; the platform and gpu features are the
engine's, and their internal headers name Ultralight types, which is why
they stay beside their sources rather than under the include root —
Ultralight is private to the library and no public header includes it.

## Using it

The threaded engine owns a background thread, repaints pages on its own
cadence, and publishes each repaint as an immutable frame. A consumer draws
whatever is current:

```cpp
#include <sigilscry/engine/WebEngine.h>
#include <sigilscry/engine/WebView.h>

using namespace sigil::scry;

auto engine = WebEngine::create();               // default config: threaded
auto view = engine->createView(1280, 720);
view->loadHTML("<html><body><h1>hello</h1></body></html>");

// Later, on your render thread, per frame:
if (view->frameVersion() != lastDrawnVersion)
  needsRedraw = true;
view->draw(canvas, SkRect::MakeWH(1280, 720));
```

To drive the engine in lockstep with your own loop instead, turn threading
off and pump it yourself:

```cpp
WebEngineConfig config;
config.threaded = false;
auto engine = WebEngine::create(config);
auto view = engine->createView(900, 620);
view->setLoadCallback([&] { loaded = true; });
view->loadHTML(html);

while (!settled) {
  engine->update();                     // timers, callbacks, network
  const bool painted = engine->renderFrame();
  // ...
}

SkPixmap live;
if (view->peekPixels(&live))            // zero-copy view of the surface
  encodePng(live);
```

Going the other way, a page reserves a slot and native code fills it:

```html
<img src="gauge.imgsrc" />
```

```cpp
auto gauge = engine->createImage("gauge", 200, 200);   // before loading the page
gauge->paint([](SkCanvas &canvas) { /* draw with Skia */ });
```

Pages also take input (`mouseMove`, `mouseDown`, `mouseUp`, `scroll`, all in
view pixels) and script (`evaluateScript`, with an optional callback
receiving the stringified result).

## The mental model

**Three objects.** `WebEngine` boots Ultralight and owns the renderer.
`WebView` is one offscreen page. `WebImage` is one drawable slot a page can
display. All three are held by `shared_ptr`, and views and images keep their
engine alive, so destruction order between handles does not matter.

**One web thread.** Everything Ultralight touches lives on it. In threaded
mode public calls are marshalled there — posted, or posted and waited on
when they must return a value — and run inline when you are already on that
thread. In unthreaded mode there is no marshalling at all: the caller's
thread *is* the web thread, which is why every call has to come from it.

**Three features, one seam each.** The platform feature is the set of
handlers Ultralight's `Platform` singleton takes: a surface whose pixel
store is an `SkBitmap` (premultiplied BGRA in sRGB, rows padded to 16
bytes for Ultralight's SIMD paint paths), a file system with a root for
the SDK's resources and a root for page content, a logger that bridges
every message to one callback, and the resource directory resolved in
order from the configured path, the folder staged next to the
executable, and the SDK location found at configure time. The gpu
feature is the driver Ultralight renders through when a device is
given: `GpuDriver` is the graphics-API-neutral contract (command
execution plus publish blits, slot textures, uploads, painting through
a Graphite recorder and wrapping a texture as an `SkImage`), and
`MetalDriver` its one implementation; a Vulkan driver joins beside it
and the engine picks it by the device's backend. The engine feature is
the web thread, the views and the slots — the only feature with a
public API beyond `LogLevel`.

**Two backends behind one API.** Leave `WebEngineConfig::gpuDevice` null
and the CPU renderer paints straight into the `SkBitmap`-backed surface
per view. Set it to the `sigil::core::hardware::GpuDevice` your renderer draws with —
owned or adopted, it is the one queue every draw rides — and Ultralight
renders through its GPU pipeline instead: the Metal driver executes its
command lists, blits each repaint into ping-pong publish textures named
on that device, and `frame(recorder)` wraps the published texture
zero-copy as a Graphite-backed `SkImage`. Every texture the engine hands
out — a frame's, a `WebImage`'s — is a `TextureHandle` on that device,
never a native object; `exportNative` there hands the object out when
some other API needs it, and a handle kept past the texture's life is
stale rather than dangling. The public API is identical either way.

**One Graphite context, one recorder per thread.** The engine's own
drawing (`WebImage::paint`) happens on the web thread, on a recorder of
its own taken from the Graphite context in `WebEngineConfig::graphite`
through `makeRecorder()`, so it carries the context's image provider and
ordered replay — raster images drawn in a paint callback upload and land
like anywhere else. A recorder belongs to one thread, but the context is
shared, and Graphite's context tolerates several threads only one at a
time: the web thread inserts and submits under `lockContext()`, and a
host that shares the context makes its own context calls — inserts,
submits, readbacks, `checkAsyncWorkCompletion` — under that lock too
(`OffscreenSurface::submit()` already does). Leave `graphite` null and
the engine makes a context of its own over the device; the one queue
still orders everything, at the price of a second context's caches.

**Frames are snapshots, not accessors.** `WebView::frame()` returns
everything about the latest repaint in one value — image, native texture,
size, dirty bounds, and a version that increases by one per repaint.
`frameVersion()` is the cheap poll for consumers that want to skip work.
On GPU engines the wrap handed out by `frame(recorder)` is cached per
(version, recorder), so the `SkImage` identity is stable across draws of
one frame and Skia's caches keyed on it stay warm.

**One renderer per process.** Ultralight allows exactly one, for the
program's lifetime. `WebEngine::create()` returns null on a second call
— which is why the CPU-mode and GPU-mode engines are tested and
benchmarked in separate binaries.

## Conventions that will bite you

**CPU frames are pooled snapshots.** Each publish copies the live surface
into an immutable buffer so consumers may hold a frame indefinitely while
Ultralight keeps painting. Buffers are recycled once every consumer
releases them, matched by exact byte size; buffers of the old size are
dropped once consumers release them, so the first publishes after a
resize allocate.

**Teardown order is load-bearing, and the library handles it — do not
rearrange it.** Platform bring-up touches the image source provider
before the renderer is created so static destruction runs in the
opposite order. The web thread's exit path clears views first, then runs
one more update, render and memory purge plus a driver flush, so deferred
GPU destroys still reach a live driver and WebCore's thread-local font
cache does not carry GPU glyph textures into thread-local cleanup. The
driver must outlive the renderer it was given.

**Smaller edges.** `loadHTML` forces a `file:///` base URL so relative
resources and image slots resolve. GPU bring-up failure is not fatal — the
engine falls back to CPU and logs a warning. A page naming a slot with no
registered `WebImage` still gets its indirection file, plus a warning
naming the slot. `update(const sk_sp<SkImage>&)` refuses texture-backed
images (they are recorder-bound); name the texture on the engine's device
and pass the handle to `updateTexture()`, or use `paint()`.

## Boundary

Public dependencies: Skia and SigilSkia — every texture the engine hands
out is a `sigil::core::hardware::TextureHandle` on the host's `GpuDevice`, and the
GPU driver draws over the host's `GraphiteContext`. Private:
`Ultralight::Ultralight`, `Ultralight::AppCore` (the engine feature
only), and Metal on Apple. No public header includes an Ultralight
header.

SigilScry brings up no device and no context of its own unless the host
shares none. It has no window, no event loop of its own beyond the web
thread, and no knowledge of scene or product content.

Sharing an `SkCanvas` with Ultralight directly is not possible — it records
its own render passes — so a texture- or bitmap-backed `SkImage` is the
compositing model, in both directions.

## Building

The library is gated on `SPELLCIRCLE_ENABLE_ULTRALIGHT`, which turns itself
off with a warning when the SDK is not found (see
`cmake/FindUltralight.cmake`), and needs the `SigilSkia` target — that is,
`SPELLCIRCLE_ENABLE_SKIA_CANVAS` on — or it is skipped with a message.

Targets: `SigilScryPlatform`, `SigilScryGpu`, `SigilScryEngine` and the
`SigilScry` umbrella. Tests (ctest): `scry_platform_test` exercises the
handlers without a renderer — the surface's format and alignment, the
file system's roots, MIME table and synthesized slot files, the logger's
routing, and the staged resource directory with the runtime probe that
answers over it; `scry_gpu_test` drives the Metal driver directly, with
no renderer and no page, and proves every upload, paint, blit and wrap
by reading pixels back through Graphite; `scry_engine_test` runs the
CPU-mode engine end to end and `scry_engine_gpu_test` the GPU-mode one,
one case per door a page-visible slot can be filled through.

The two GPU binaries exist only on Apple and every case in them needs a
device, so they carry the ctest label `gpu`: a machine without one shows
them as a lane not run rather than as a lane that passed. Both take the
device, the shared Graphite context and the asynchronous surface read
from `test/GraphiteReadback.h`; the two engine binaries take the page
waits from `engine/test/Wait.h`, where a wait that expires says so and
names what it was waiting for, rather than reporting the colour a page
never painted. They stay two binaries because a process gets one
renderer. Benchmarks (Google
Benchmark, through the `benches` target and `scripts/bench_ledger.py`):
`scry_platform_bench`, `scry_gpu_bench` (Apple), and `scry_engine_bench`
— `--gpu` runs the latter's GPU-mode arms, a separate run because of the
one-renderer rule; the ledger runs the CPU mode.

New executables that link `SigilScry` must also call
`ultralight_copy_resources(<target>)` in their `CMakeLists.txt`.

### Installing the SDK (macOS)

1. Download the Free SDK 1.4.x for your architecture from
   <https://ultralig.ht> (e.g. `ultralight-free-sdk-1.4.0-mac-arm64.7z`)
   and extract it.

2. Install headers and dylibs to `/usr/local`, the prefix
   `cmake/FindUltralight.cmake` searches:

   ```sh
   sudo cp -R <sdk>/include/Ultralight /usr/local/include/
   sudo cp -R <sdk>/include/AppCore /usr/local/include/Ultralight/
   sudo cp -R <sdk>/include/JavaScriptCore /usr/local/include/Ultralight/
   sudo cp <sdk>/bin/libUltralight.dylib <sdk>/bin/libUltralightCore.dylib \
           <sdk>/bin/libWebCore.dylib <sdk>/bin/libAppCore.dylib /usr/local/lib/
   ```

   Note the nesting: `AppCore/` and `JavaScriptCore/` go *inside*
   `/usr/local/include/Ultralight/`, because includes are spelled
   `<Ultralight/AppCore/...>`.

3. Re-sign the dylibs. They ship with quarantine attributes that make dyld
   refuse to load them ("code signature not valid for use in process"):

   ```sh
   sudo xattr -d com.apple.quarantine /usr/local/lib/lib{Ultralight,UltralightCore,WebCore,AppCore}.dylib
   sudo codesign --force --sign - /usr/local/lib/lib{Ultralight,UltralightCore,WebCore,AppCore}.dylib
   ```

   `codesign` may print "internal error in Code Signing subsystem" and still
   succeed — check with `codesign -v`.

4. Install the runtime resources (`icudt67l.dat` and `cacert.pem`).
   Ultralight distributes these with the application rather than with the
   dylibs, so put the SDK archive's `resources/` folder somewhere
   `FindUltralight` looks:

   ```sh
   # machine-global…
   sudo mkdir -p /usr/local/share/ultralight
   sudo cp -R <sdk>/resources /usr/local/share/ultralight/

   # …or per-user, no sudo needed
   mkdir -p "$HOME/Library/Application Support/Ultralight"
   cp -R <sdk>/resources "$HOME/Library/Application Support/Ultralight/"
   ```

   Alternatively, skip this and point CMake at the extracted SDK with
   `-DULTRALIGHT_SDK_DIR=<sdk>`.

### Resources at run time

`ultralight_copy_resources(<target>)` stages the resource folder next to the
built binary after every build (`<exe dir>/resources`) — Ultralight's
standard app-bundling layout, and how a packaged application should ship
them. At startup the engine resolves the resource directory in this order:

1. `WebEngineConfig::resourceDir`, if set,
2. `resources/` next to the executable — the staged copy,
3. the SDK location found at configure time, compiled in as a fallback.

Missing resources are what an engine failing to boot usually means, and
`runtime::available(&why)` in `<sigilscry/platform/Runtime.h>` answers
whether they are there BEFORE an engine exists — a process is allowed
exactly one renderer, so a caller that wants to ask first must be able to
ask without spending it.
