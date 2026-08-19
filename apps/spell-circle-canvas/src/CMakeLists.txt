# Source tree layout:
#
#   common/      libraries shared across products: skia/ (Graphite GPU
#                plumbing), ui/ (Ifrit.Ui Qt Quick controls), image/
#                (SigilImage), loader/ (SigilLoader), scry/ (SigilScry),
#                motion/ (SigilMotion), shape/ (SigilShape), world/
#                (SigilWorld), compose/ (SigilCompose)
#   sigilweave/  the Qt-free SigilWeave text layout engine, its companion
#                libraries, tests, benchmarks, and examples/ (gallery, demo)
#   spellcircle/ the SpellCircle receiver product: shared/ core embedded by
#                both the qt/ and mac/ executables
if(SPELLCIRCLE_ENABLE_SKIA_CANVAS)
  add_subdirectory(common/skia)
endif()
add_subdirectory(common/ui)
add_subdirectory(common/image)
add_subdirectory(common/loader)
if(SPELLCIRCLE_ENABLE_ULTRALIGHT)
  add_subdirectory(common/scry)
endif()
add_subdirectory(common/motion)
add_subdirectory(common/shape)
add_subdirectory(common/world)
if(SPELLCIRCLE_ENABLE_SUBSTANCE)
  add_subdirectory(common/substance)
endif()
if(SPELLCIRCLE_ENABLE_USD)
  add_subdirectory(common/usd)
endif()
add_subdirectory(common/compose)
add_subdirectory(sigilweave)
add_subdirectory(spellcircle)
