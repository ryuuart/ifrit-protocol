# SigilUsd — the world's data in and out of USD (OpenUSD, core only), as
# one feature library per directory. The runtime leaf says whether the
# USD plugins are present; write builds a stage from the values a scene is
# made of and saves it; read pours a stage into geometry's decode::Model.
# `SigilUsd` is the umbrella over all three. A leaf of the tree: it links
# SigilGeometry, SigilMaterial and the world's light values; nothing links
# it but its tests, its demo hooks and whoever wants a stage.
#
# Every public header lives under one include root, include/sigilusd/
# <feature>/, so a feature adds the root and spells its own directory.
# <sigilusd/Usd.h> is the umbrella header. Added only when
# SPELLCIRCLE_ENABLE_USD is on and vcpkg's usd port was found.
find_package(pxr CONFIG REQUIRED)

set(SIGIL_USD_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

# Dependencies point up this list; write and read are independent of
# each other and both stand on nothing in this library but the runtime
# probe their tests skip on.
add_subdirectory(runtime)
add_subdirectory(write)
add_subdirectory(read)

add_library(SigilUsd INTERFACE)
target_link_libraries(SigilUsd INTERFACE SigilUsdRuntime SigilUsdWrite SigilUsdRead)

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilUsd
  BRIEF "The world's data written to and read back from USD stages"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
