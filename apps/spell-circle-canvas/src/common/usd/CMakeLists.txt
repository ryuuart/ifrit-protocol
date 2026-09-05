# SigilUsd — the world's data in and out of USD (OpenUSD, core only), as
# one feature library per directory. The runtime leaf says whether the
# USD plugins are present; write builds a stage from the values a scene is
# made of and saves it; read pours a stage into geometry's decode::Model.
# `SigilUsd` is the umbrella over all three. A leaf of the tree: it links
# SigilGeometry, SigilMaterial and the world's light values; nothing links
# it but its tests, its demo hooks and whoever wants a stage.
#
# <sigilusd/Usd.h> is the umbrella header. Added only when
# SPELLCIRCLE_ENABLE_USD is on and vcpkg's usd port was found. The
# fixtures every binary here shares — the scratch path, the runtime skip
# and the scene both doors are exercised over — sit in test/.
find_package(pxr CONFIG REQUIRED)

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
