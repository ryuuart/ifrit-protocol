# Authoring values and renderer executors are separate feature targets.
set(material_chapters
  ${CMAKE_CURRENT_SOURCE_DIR}/ADVANCED.md
  ${CMAKE_CURRENT_SOURCE_DIR}/COLOUR.md
  ${CMAKE_CURRENT_SOURCE_DIR}/PAINT.md
  ${CMAKE_CURRENT_SOURCE_DIR}/SUBSTANCE.md
  ${CMAKE_CURRENT_SOURCE_DIR}/reference/VALUES.md)

sigil_library_root(SigilMaterial
  BRIEF "Comparable materials from colors, layers, shaders and surface responses"
  DOCS ${CMAKE_CURRENT_SOURCE_DIR}/docs/Groups.dox ${material_chapters})

# Feature dependencies follow this order.
add_subdirectory(color)
add_subdirectory(core)
add_subdirectory(texture)
add_subdirectory(mask)
add_subdirectory(ocio)
add_subdirectory(substance)
add_subdirectory(sdf)
add_subdirectory(pattern)
add_subdirectory(field)
add_subdirectory(skia)
add_subdirectory(program)
add_subdirectory(surface)
add_subdirectory(slang)

sigil_library(SigilMaterial
  INTERFACE SigilMaterialColor SigilMaterialCore SigilMaterialTexture
            SigilMaterialMask SigilMaterialOcio SigilMaterialSdf
            SigilMaterialPattern SigilMaterialField SigilMaterialSkia
            SigilMaterialProgram SigilMaterialSurface
            SigilMaterialSlang)

# Feature cases share one test binary; consumers still select feature archives.
sigil_test(material_test
  SOURCES color/test/ColorTest.cpp color/test/ExtractTest.cpp
          color/test/RampTest.cpp field/test/FieldTest.cpp
          ocio/test/OcioTest.cpp pattern/test/PatternTest.cpp
          pattern/test/WeaveTest.cpp sdf/test/SdfTest.cpp
  SUPPORT_DIRS ${SIGIL_SHADER_GENERATED_DIR}
  DEFINITIONS SIGIL_MATERIAL_SDF_SHADER_DIR="${SIGIL_MATERIAL_SDF_SHADER_DIR}"
              SIGIL_MATERIAL_FIELD_SHADER_DIR="${SIGIL_MATERIAL_FIELD_SHADER_DIR}"
              SIGIL_MATERIAL_PATTERN_SHADER_DIR="${SIGIL_MATERIAL_PATTERN_SHADER_DIR}"
  LIBRARIES SigilMaterialColor SigilMaterialField SigilMaterialOcio
            SigilMaterialPattern SigilMaterialSdf SigilMaterialSkia
  SUITES Ocio LABELS ocio)

sigil_doc_probes(material
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${material_chapters}
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/enums/Backface.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/functions/over.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/functions/shader.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Color.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Filter.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Material.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Paint.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Palette.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Ramp.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/amount.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/bleed.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/fit.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/offset.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/quantizeTime.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/slot.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/bind.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/verbs/worldSpace.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../motion/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../io/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../media/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../skia/include
  PRELUDES "<sigilio/hub/Hub.h>" "<sigilmedia/core/Image.h>"
  FLOORS 85,69,85,67,3
  LIBRARIES SigilMaterial SigilMaterialSubstance SigilIOHub)

# Device compilation also checks shader bodies after Graphite inlines them.
if(APPLE)
  sigil_test(material_test
    SOURCES test/MaterialGpuTest.mm
    LIBRARIES SigilMaterial SigilSkia
    SUITES MaterialGpu LABELS gpu)
endif()
