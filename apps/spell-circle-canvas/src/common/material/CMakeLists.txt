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
# no renderer. The colour feature is the leaf under the core: the value
# a params struct holds and the OKLab round trip, linking nothing. The
# sdf, pattern and field features are the primitives — fully
# parameterised generators — the ocio feature bakes OpenColorIO's view
# transforms to LUT materials over the texture feature, and the kit
# holds the presets over them. `SigilMaterial` is the umbrella: an
# interface over every feature, for a consumer of the whole library.
# <sigilmaterial/Material.h> is the umbrella header over the core.
sigil_library_root(SigilMaterial
  BRIEF "Materials as recipe instances: reflected params, per-target bodies, one program cache")

# Dependencies point up this list.
add_subdirectory(color)
add_subdirectory(core)
add_subdirectory(texture)
add_subdirectory(ocio)
add_subdirectory(sdf)
add_subdirectory(pattern)
add_subdirectory(field)
add_subdirectory(skia)
add_subdirectory(kit)
add_subdirectory(slang)

sigil_library(SigilMaterial
  INTERFACE SigilMaterialColor SigilMaterialCore SigilMaterialTexture
            SigilMaterialOcio SigilMaterialSdf SigilMaterialPattern
            SigilMaterialField SigilMaterialSkia SigilMaterialKit
            SigilMaterialSlang)

# THE DEVICE SWEEP, over the whole library rather than one feature: a body
# is validated one way as its own runtime effect and another once a GPU
# backend has inlined it into a pipeline, and every per-feature suite
# compiles on a raster surface, so none of them can see the second. It
# links the umbrella and Graphite, which is why it stands here and not
# under a feature.
if(APPLE)
  sigil_test(material_gpu_test
    SOURCES test/MaterialGpuTest.mm
    LIBRARIES SigilMaterial SigilSkia
    LABELS gpu)
endif()
