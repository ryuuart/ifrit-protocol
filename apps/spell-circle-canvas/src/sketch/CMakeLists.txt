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
  BRIEF "Sketches: one file per scene, one registry, one host"
  DOCS ${CMAKE_CURRENT_LIST_DIR}/kit/README.md)
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

# The sheet a sketch stands on: one theme value bound for a scope, and
# the furniture — the stage, the page, the well, the caption — every
# specimen sheet would otherwise retype. It decides no look: the theme is
# a seam and the house one is a stock value over it.
add_subdirectory(kit)

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

# Every sketch stem a document names, resolved against the registry.
# A README points its reader at the sketches that draw the feature beside
# it and spells their stems by hand, so a rename in the registry leaves
# prose pointing at a file that is not there with nothing to notice. This
# is the registry's check — it is the registry the stems are resolved
# against — so it lives here and reads every document in the tree.
file(GLOB_RECURSE SIGIL_SKETCH_STEM_DOCS CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_LIST_DIR}/../README.md
     ${CMAKE_CURRENT_LIST_DIR}/../FEATURES.md
     ${CMAKE_CURRENT_LIST_DIR}/../TYPOGRAPHY.md)
set(SIGIL_SKETCH_STEM_DOC_ARGS)
foreach(doc IN LISTS SIGIL_SKETCH_STEM_DOCS)
  list(APPEND SIGIL_SKETCH_STEM_DOC_ARGS --doc ${doc})
endforeach()
add_test(NAME sketch_readme_stems
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_CURRENT_LIST_DIR}/test/readme_sketch_stems.py
          --sketch-dir ${SIGIL_SKETCH_DIR}
          ${SIGIL_SKETCH_STEM_DOC_ARGS})

# The checker's own fixtures: a stem must resolve, a misspelt one must
# fail the run, a token outside a sketch paragraph must not be read as a
# stem, and a count of a list must be refused. One Python invocation — and
# the only check that notices the EXTRACTOR narrowing, since a checker that
# silently resolves fewer stems still passes over the corpus.
add_test(NAME sketch_readme_stems_self_test
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_CURRENT_LIST_DIR}/test/readme_sketch_stems.py --self-test)

# The umbrella, for a consumer that takes the library rather than a
# feature of it.
sigil_library(SigilSketch
  INTERFACE SigilSketchCore SigilSketchCanvas SigilSketchSet SigilSketchDraw
            SigilSketchKit SigilSketchLive SigilSketchPlate)
