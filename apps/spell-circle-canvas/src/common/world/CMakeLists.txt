# SigilWorld — 3D scenes as comparable values, executed on a GPU through
# Diligent Engine (Vulkan/MoltenVK on macOS) over a device SigilSkia
# shares. Each feature is its own static archive with its own tests and
# benchmarks, and links only the features beneath it, so a consumer that
# needs one links that one alone.
#
# Every public header lives under one include root, include/sigilworld/,
# and the directories under it nest the way the targets do, so a feature
# adds the root and spells its own directory.
set(SIGIL_WORLD_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

# The light feature stands alone: emitter VALUES over glm, with no
# device and no registry, so a consumer that only needs to say where the
# lights are links it without anything that draws.
add_subdirectory(light)

# The device feature needs SigilSkia's, since the whole point of it is
# that both APIs draw on one device. The light feature above stands
# without it.
if(TARGET SigilSkiaDevice)
  add_subdirectory(diligent)
else()
  message(STATUS
    "SigilWorldDiligent skipped: needs SPELLCIRCLE_ENABLE_SKIA_CANVAS "
    "(SigilSkiaDevice)")
endif()

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilWorld
  BRIEF "3D scenes as values, executed on a GPU over a shared device"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
