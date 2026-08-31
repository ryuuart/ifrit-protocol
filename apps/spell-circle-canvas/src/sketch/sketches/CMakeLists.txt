# The sketches, compiled into whichever host links them.
#
# A SKETCH JOINS BY BEING A FILE HERE. The stem is its key, and
# SIGIL_SKETCH_STATIC — set per source below — switches the registration
# macro from exporting a dylib's entry points to registering a factory
# under that stem. So the file someone hot-reloads and the sketch a host
# shows are the same file, always, and there is no second list anywhere
# that could disagree with this directory.
#
# An OBJECT library, not an archive: registration happens in a
# namespace-scope initializer with nothing referring to it, which an
# archive would drop. Object libraries link every object by
# construction — no -force_load, and every host shares one compile.
file(GLOB SIGIL_SKETCH_FILES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
list(REMOVE_ITEM SIGIL_SKETCH_FILES ${CMAKE_CURRENT_LIST_DIR}/Anchor.cpp)
foreach(sketch_file IN LISTS SIGIL_SKETCH_FILES)
  get_filename_component(sketch_stem ${sketch_file} NAME_WE)
  set_source_files_properties(${sketch_file} PROPERTIES
    COMPILE_DEFINITIONS "SIGIL_SKETCH_STATIC=\"${sketch_stem}\"")
endforeach()

add_library(SigilSketches OBJECT
  Anchor.cpp
  ${SIGIL_SKETCH_FILES}
)
# THE SKETCH API SURFACE, stated once. What a sketch may #include is
# exactly this target's PUBLIC dependencies, and the flags a hot-reloaded
# sketch compiles with are lifted from Anchor.cpp's entry in the
# compilation database — which is a source of THIS target. So the header
# surface a compiled-in sketch sees and the one a hot-reloaded sketch
# sees cannot drift apart.
target_link_libraries(SigilSketches PUBLIC
  SigilSketchCanvas SigilSketchSet)
target_compile_definitions(SigilSketches PUBLIC
  SIGIL_SKETCH_ASSET_DIR="${SIGIL_SKETCH_ASSET_DIR}"
  SIGIL_SKETCH_DIR="${SIGIL_SKETCH_DIR}"
  SIGIL_ASSET_DIR="${SIGIL_ASSET_DIR}")
set_target_properties(SigilSketches PROPERTIES POSITION_INDEPENDENT_CODE ON)
