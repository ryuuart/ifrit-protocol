# IfritWeb — Ultralight SDK setup

IfritWeb renders HTML/CSS/JS onto the scene canvases through the
[Ultralight](https://ultralig.ht) SDK (WebKit-derived). The build needs
the SDK installed on the machine; without it, CMake prints a warning and
disables the backend (`SPELLCIRCLE_ENABLE_ULTRALIGHT` turns itself off).

## Installing the SDK (macOS)

1. Download the Free SDK 1.4.x for your architecture from
   <https://ultralig.ht> (e.g. `ultralight-free-sdk-1.4.0-mac-arm64.7z`)
   and extract it.

2. Install headers and dylibs to `/usr/local` (the prefix
   `cmake/FindUltralight.cmake` searches):

   ```sh
   sudo cp -R <sdk>/include/Ultralight /usr/local/include/
   sudo cp -R <sdk>/include/AppCore /usr/local/include/Ultralight/
   sudo cp -R <sdk>/include/JavaScriptCore /usr/local/include/Ultralight/
   sudo cp <sdk>/bin/libUltralight.dylib <sdk>/bin/libUltralightCore.dylib \
           <sdk>/bin/libWebCore.dylib <sdk>/bin/libAppCore.dylib /usr/local/lib/
   ```

   Note the nesting: this repo expects `AppCore/` and `JavaScriptCore/`
   *inside* `/usr/local/include/Ultralight/` (includes are spelled
   `<Ultralight/AppCore/...>`).

3. Re-sign the dylibs. The SDK ships them with quarantine/provenance
   attributes that make dyld refuse to load them ("code signature not
   valid for use in process"):

   ```sh
   sudo xattr -d com.apple.quarantine /usr/local/lib/lib{Ultralight,UltralightCore,WebCore,AppCore}.dylib
   sudo codesign --force --sign - /usr/local/lib/lib{Ultralight,UltralightCore,WebCore,AppCore}.dylib
   ```

   (`codesign` may print "internal error in Code Signing subsystem" and
   still succeed — verify with `codesign -v`.)

4. Install the runtime resources (ICU data + CA certificates). Per
   Ultralight's docs these are distributed with the application, not with
   the dylibs; put the SDK archive's `resources/` folder in one of the
   locations FindUltralight searches:

   ```sh
   # machine-global (preferred), or…
   sudo mkdir -p /usr/local/share/ultralight
   sudo cp -R <sdk>/resources /usr/local/share/ultralight/

   # …per-user, no sudo needed
   mkdir -p "$HOME/Library/Application Support/Ultralight"
   cp -R <sdk>/resources "$HOME/Library/Application Support/Ultralight/"
   ```

   Alternatively, skip this step and point CMake at the extracted SDK:
   `python3 scripts/setup.py … && cmake -DULTRALIGHT_SDK_DIR=<sdk> build`.

## Resources at build and run time

Every executable that links `IfritWeb` calls the
`ultralight_copy_resources(<target>)` CMake function, which stages the
resources next to the built binary (`<exe dir>/resources`) after each
build — Ultralight's standard app-bundling layout, and how a packaged
app should ship them. At runtime the engine resolves the resource
directory in this order:

1. `WebEngineConfig::resourceDir` (explicit override),
2. `resources/` next to the executable (the staged copy),
3. the SDK install location found at configure time.

New executables using the backend should link `IfritWeb` and call
`ultralight_copy_resources(<target>)` in their `CMakeLists.txt`.

## Image slots (Skia content inside pages)

Pages declare native-drawable slots wherever an image URL is accepted:

```html
<img src="sigil.imgsrc" />
```

The native side registers the slot and draws into it:

```cpp
auto sigil = engine->createImage("sigil", 200, 200);   // before loading the page
sigil->paint([](SkCanvas &canvas) { /* draw with Skia */ });
```

`paint()` is the safe path for runtime collaboration: the engine hands
you a canvas already targeting the slot's backing store (a Graphite
surface on the engine's own recorder in GPU mode, the shared bitmap in
CPU mode) and performs the GPU flush and page invalidation in the same
step, serialized on the web thread — there is no wrap, submit, or
invalidate to forget, and pages can never observe a half-drawn slot.
Call it once for static content or per tick for animation. `update()`
copies raster pixels instead; the raw `nativeTexture()` remains available
for expert integrations (draw with your own recorder on the engine's
device/queue, submit, then `invalidate()` — in that order). A page that
references a slot name with no registered WebImage logs a warning naming
the slot.

## Performance

`web_bench` (Release) measures the integration paths; run it plain for
the CPU engine and with `--gpu` for the GPU engine (one Ultralight
renderer per process, so the modes are separate runs). Reference numbers
from an Apple Silicon dev Mac, 1280x720 view, per operation:

| Path                                   | CPU engine      | GPU engine |
| -------------------------------------- | --------------- | ---------- |
| `frame()` acquire                      | 13 ns           | 9 ns |
| full-view draw onto the scene canvas   | 571 µs          | 1.4 µs record, 56 µs with per-frame submit (14 µs CPU) |
| `frameImage()` texture wrap            | n/a (raster)    | 231 ns |
| slot `update()` raster 256² / 1024²    | 14 µs / 136 µs  | 42 µs / 474 µs |
| slot `updateTexture()` 256² / 1024²    | n/a             | 31 µs / 38 µs |
| slot `paint()` 256² / 1024²            | 62 µs / 198 µs  | 61 µs / 61 µs |
| DOM change → published frame           | 22 ms           | 22 ms |

Guidance that falls out of this:

- GPU mode makes consumer-side compositing ~400x cheaper (a recorded
  textured quad vs a 3.7 MB raster blit) and the SkImage wrap is free —
  draw web frames every scene frame without budgeting for them.
- On GPU engines, feed slots with `paint()` or `updateTexture()` (cost
  is size-independent — a blit/draw on the shared queue) rather than
  raster `update()`, which pays a CPU convert + upload.
- The ~22 ms DOM-change latency is dominated by pacing — Ultralight's
  Free-edition 60 FPS repaint cap plus the engine's own default 60 FPS
  render cadence — not by compositing cost. Input-to-glass latency for
  web content is therefore roughly two frames.

## GPU vs CPU

Pass the host's `id<MTLDevice>` / `id<MTLCommandQueue>` (bridged to
`void*`) in `WebEngineConfig::metalDevice` / `metalCommandQueue` to render
through the Metal `GPUDriver` and consume frames as texture-backed
Graphite `SkImage`s; leave them null for the CPU renderer and raster
`SkImage` frames. See the architecture notes in the repo's CLAUDE.md and
the `examples/` demos for both modes.
