set(chapter Contract.md)
if(MISSING_CHAPTER)
  set(chapter MissingChapter.md)
endif()
sigil_library_root(SigilHelpersFixture
  BRIEF "Consumer language and documentation path contracts"
  DOCS ${chapter})

sigil_library(FixtureCompiled SOURCES Compiled.cpp HEADERS Compiled.h)
sigil_library(FixtureInterface HEADERS Interface.h)
sigil_library(FixtureShaded SOURCES Shaded.cpp HEADERS Shaded.h)
sigil_shader_sources(FixtureShaded
  DIR ${CMAKE_CURRENT_LIST_DIR}/shaders NAMESPACE sigil::fixture)
