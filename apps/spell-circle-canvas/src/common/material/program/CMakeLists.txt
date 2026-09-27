# The program feature: a recipe body in a file, read through the resource
# hub and instanced as a material. It stands apart from the core because
# it links the hub; the core's value model reads no files.
sigil_library(SigilMaterialProgram
  SOURCES Program.cpp
  HEADERS Program.h
  PUBLIC SigilMaterialCore SigilIOHub)

sigil_test(material_test
  SOURCES test/ProgramTest.cpp
  LIBRARIES SigilMaterialProgram SigilMaterialSkia SigilIOHub)
