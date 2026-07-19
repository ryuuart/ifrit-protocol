# Source tree layout:
#
#   common/      libraries shared across products: the generic Skia Graphite
#                GPU plumbing (skia/), the reusable Ifrit.Ui Qt Quick
#                controls (ui/), and image import for the canvases (image/)
#   textflow/    the Qt-free TextFlow text layout engine, its companion
#                libraries, tests, benchmarks, and examples/ (gallery, demo)
#   spellcircle/ the SpellCircle receiver product: shared/ core embedded by
#                both the qt/ and mac/ executables
if(SPELLCIRCLE_ENABLE_SKIA_CANVAS)
  add_subdirectory(common/skia)
endif()
add_subdirectory(common/ui)
add_subdirectory(common/image)
if(SPELLCIRCLE_ENABLE_ULTRALIGHT)
  add_subdirectory(common/web)
endif()
add_subdirectory(common/poster)
add_subdirectory(textflow)
add_subdirectory(spellcircle)
