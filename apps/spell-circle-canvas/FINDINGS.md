# Findings

## Billboard point clouds are substantially slower on the GPU lane

`pop_billboards` completes its raster plate, but its headless GPU sweep takes
long enough to exceed a 150-second capture timeout. An isolated GPU sweep
eventually completes with the same visible composition. Sampling the process
shows Graphite's Vulkan submission and fence waits dominating the main thread.
The point renderer in `src/common/geometry/mesh/pop/Billboards.cpp` creates a
tint color filter and submits one image rectangle for each splat.

Reproduce both paths with:

```sh
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  --headless /tmp/billboards-gpu --gpu --sketch pop_billboards
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  --headless /tmp/billboards-raster --sketch pop_billboards
```

The GPU path should handle dense clouds through batches of compatible sprites
while preserving depth order, per-point tint and size, atlas windows, and the
requested blend mode. A renderer benchmark should cover the default soft dot,
the ring sprite, and atlas windows at increasing cloud sizes. Plate comparisons
should assert that batching preserves those visual properties on raster and
GPU backends. Runtime measurements belong to the benchmark results.

## Sketchbook's OpenGL raster fallback presents the canvas upside down

With `QSG_RHI_BACKEND=opengl`, Sketchbook uploads its raster frame into the
Qt Quick texture with the opposite vertical orientation from the displayed
canvas. The sketch is upside down while the surrounding controls remain
upright. The Metal path displays the same sketch correctly.

The raster and GPU paths should present the same top-to-bottom orientation.
Reproduce with:

```sh
QSG_RHI_BACKEND=opengl build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  --sketch spacing_passes --window-bench 30 --window-size 900x600 \
  --shot /tmp/sketchbook-opengl.png
```

A window regression should render an asymmetric scene with distinct top and
bottom markers through the OpenGL raster fallback, capture the presented
window, and assert that the markers have the same orientation as the raster
reference. The upload and presentation meet in
`src/sketch/book/SketchbookRenderer.cpp`.

## Closing a shared macOS window can fault during observer cleanup

An interactive Sketchbook session can exit with `SIGSEGV` after publication
is started, stopped, and started again while the macOS window-sharing overlay
is present. The captured stack enters Foundation's
`_NSKeyValueRetainedObservationInfoForObject` from ViewBridge's
`NSRemoteView` observer cleanup, through `NSWindow` ordering out and Qt's
window destruction. The publication request and the sharing overlay are
observed conditions, not established causes.

Quit should release the window and publisher without a fault. Separate
stopped and publishing sessions both exit successfully through the same
Quit action, so the failure is intermittent and its trigger remains
unresolved. A reproduction should exercise publication toggles with an
active macOS window-sharing overlay, then assert that quitting completes
with exit status zero. Inspect the native window's observation and class
lifetime if that sequence reproduces the fault; the shared background
rendering helper changes its Objective-C class.
