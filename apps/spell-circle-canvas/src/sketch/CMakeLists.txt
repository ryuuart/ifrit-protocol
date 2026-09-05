# SigilSketch — everything renderable in this repository is a sketch: one
# file that declares a scene, addressed by its own stem, driven by one
# registry, one live host and one headless renderer.
#
# It stands ABOVE the drawing libraries rather than beside them: a sketch
# may describe a compose Element tree, a world Frame or a pen's frame, so
# this library links all three. Nothing underneath links back — a drawn
# tree, a lit set and a pen's canvas meet here and nowhere else.
#
# Each feature is its own archive with its own tests and benchmarks, and
# links only the features beneath it. The fixtures every sketch test
# binary shares — the one font context and the one asset store a process
# holds — are test/Support.h.
sigil_library_root(SigilSketch
  BRIEF "Sketches: one file per scene, one registry, one host")
# Where a sketch looks for what it did not generate, and where the live
# host finds the file behind a registry entry. Neither directory need
# exist: a missing asset yields the placeholder and heals when a file
# appears.
set(SIGIL_SKETCH_DIR ${CMAKE_CURRENT_LIST_DIR}/sketches)
set(SIGIL_SKETCH_ASSET_DIR ${CMAKE_CURRENT_LIST_DIR}/sketches/assets)

# The surface a sketch is written against, and the registry it joins:
# what a sketch is, what it declares about its canvas, what it reaches
# for, and the seam that says which runtime draws it. No runtime here —
# this feature could not draw a pixel.
add_subdirectory(core)

# The three runtimes a sketch may be written for. Each owns its sketch
# body, its declaration surface and the session that drives it, and they
# know nothing of each other.
add_subdirectory(canvas)
add_subdirectory(set)
add_subdirectory(draw)

# The opt-in bridge for a host that lets several sketches borrow the one
# WebEngine Ultralight permits in a process. SigilScry itself keeps explicit
# caller ownership as its ordinary path; only a host that configures this
# feature gets a shared engine.
if(TARGET SigilScry)
  add_subdirectory(scry)
endif()

# The live-reload engine: a file watched, rebuilt into a dylib on save,
# and hot-swapped into the running session.
add_subdirectory(live)

# The headless renderer: every sketch stepped to its declared moment and
# photographed, with the timing table beside it.
add_subdirectory(plate)

# The sketches themselves, compiled into whichever host links them.
add_subdirectory(sketches)

# Sketchbook: the one live application.
add_subdirectory(book)

# The umbrella, for a consumer that takes the library rather than a
# feature of it.
sigil_library(SigilSketch
  INTERFACE SigilSketchCore SigilSketchCanvas SigilSketchSet SigilSketchDraw
            SigilSketchLive SigilSketchPlate)
