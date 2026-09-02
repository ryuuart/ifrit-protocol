# SigilMaterial — materials as recipe instances, as one feature library per
# directory. The core feature is the value model: a recipe (a reflected
# params struct plus one body per shading language), the material that
# instantiates it, and the one program cache that compiles through the
# compilers backends register. There are two of those backends: the Skia
# one compiles a recipe's SkSL body into an SkRuntimeEffect, the Slang one
# compiles its Slang body to SPIR-V and reports the layout a device
# renderer writes uniforms at. The texture feature is the image side: a
# texture as a comparable leaf, the tools' texture sets, the environment
# and bevel producers, the atlas. Each backend is its own feature that
# links the core and its renderer, so a consumer of the model alone links
# no renderer. The colour, sdf, pattern and field features are the
# primitives — fully parameterised generators — and the kit holds the
# presets over them. `SigilMaterial` is the umbrella: an interface over
# every feature, for a consumer of the whole library.
#
# Every public header lives under one include root, include/sigilmaterial/
# <feature>/, so a feature adds the root and spells its own directory.
# <sigilmaterial/Material.h> is the umbrella header over the core.

set(SIGIL_MATERIAL_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

# Dependencies point up this list.
add_subdirectory(core)
add_subdirectory(texture)
add_subdirectory(color)
add_subdirectory(sdf)
add_subdirectory(pattern)
add_subdirectory(field)
add_subdirectory(skia)
add_subdirectory(kit)
add_subdirectory(slang)

add_library(SigilMaterial INTERFACE)
target_link_libraries(SigilMaterial INTERFACE
  SigilMaterialCore SigilMaterialTexture SigilMaterialColor SigilMaterialSdf
  SigilMaterialPattern SigilMaterialField SigilMaterialSkia SigilMaterialKit
  SigilMaterialSlang)

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilMaterial
  BRIEF "Materials as recipe instances: reflected params, per-target bodies, one program cache"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
