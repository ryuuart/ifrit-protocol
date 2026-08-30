# Source tree layout:
#
#   common/      libraries shared across products: skia/ (SigilSkia,
#                Graphite bring-up), ui/ (Ifrit.Ui Qt Quick controls), image/
#                (SigilImage), loader/ (SigilLoader), scry/ (SigilScry),
#                measure/ (SigilMeasure), motion/ (SigilMotion), core/ (SigilCore),
#                geometry/ (SigilGeometry), world/ (SigilWorldLight,
#                SigilWorldDiligent), material/ (SigilMaterial), compose/
#                (SigilCompose)
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
add_subdirectory(common/measure)
add_subdirectory(common/motion)
add_subdirectory(common/core)
add_subdirectory(common/geometry)
add_subdirectory(common/material)
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
