# SigilUsd — the world's data in and out of USD (OpenUSD, core only), as
# one feature library per directory. The runtime leaf says whether the
# USD plugins are present; write builds a stage from the values a scene is
# made of and saves it; read pours a stage into geometry's decode::Model.
# `SigilUsd` is the umbrella over all three. A leaf of the tree: it links
# SigilGeometry, SigilMaterial and the world's light values; nothing links
# it but its tests, its demo hooks and whoever wants a stage.
#
# <sigilusd/Usd.h> is the umbrella header. The fixtures every binary here
# shares — the scratch path, the runtime skip and the scene both doors are
# exercised over — sit in test/.
#
# OpenUSD is an optional dependency of the manifest; without it the whole
# library is left out of the build.
find_package(pxr CONFIG QUIET)
if(NOT pxr_FOUND)
  message(STATUS "OpenUSD (pxr) not found -- SigilUsd is left out")
  return()
endif()

sigil_library_root(SigilUsd
  BRIEF "The world's data written to and read back from USD stages")

# Dependencies point up this list; write and read are independent of
# each other and both stand on nothing in this library but the runtime
# probe their tests skip on.
add_subdirectory(runtime)
add_subdirectory(write)
add_subdirectory(read)

sigil_library(SigilUsd
  INTERFACE SigilUsdRuntime SigilUsdWrite SigilUsdRead)
