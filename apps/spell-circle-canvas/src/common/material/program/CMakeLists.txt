# The program feature: a shader as a material — from its source text, or
# from a file read through the resource hub — with the textures it samples
# placed in its slots. It stands apart from the core because it links the
# hub and the texture feature; the core's value model reads no files and
# binds no pictures.
sigil_library(SigilMaterialProgram
  SOURCES Shader.cpp
  HEADERS Shader.h
  PUBLIC SigilMaterialCore SigilMediaCore
  PRIVATE SigilMaterialTexture SigilIOHub)

sigil_test(material_test
  SOURCES test/ShaderTest.cpp
  DEFINITIONS SIGIL_MATERIAL_INCLUDE_DIR="${SIGIL_INCLUDE_DIR}"
  LIBRARIES SigilMaterialProgram SigilMaterialSkia SigilMaterialTexture
            SigilIOHub)
