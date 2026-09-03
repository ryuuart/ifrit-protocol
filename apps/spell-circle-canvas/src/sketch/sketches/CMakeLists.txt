# The sketches, compiled into whichever host links them.
#
# A SKETCH JOINS BY BEING A FILE HERE — OR A DIRECTORY. `<stem>.cpp` is a
# sketch of one translation unit. `<stem>/<stem>.cpp` is the ENTRY of a
# sketch that is a directory: every other `.cpp` beside the entry is a
# unit of the same sketch, compiled into this target with it and carrying
# no key of its own, and every header beside it is reached by a quoted
# include. Either way the stem is the key, and SIGIL_SKETCH_STATIC — set
# on each entry below — switches the registration macro from exporting a
# dylib's entry points to registering a factory under that stem. So the
# file someone hot-reloads and the sketch a host shows are the same file,
# always, and there is no second list anywhere that could disagree with
# this directory.
#
# `shared/` IS THE ONE DIRECTORY HERE THAT IS NOT A SKETCH: modules more
# than one sketch reaches for and no library owns yet. Its sources are
# compiled into this target once, and its headers are reached as
# <shared/Name.h> — this directory is on the include path, so the include
# names the layer it comes from. A module a library should own is
# promoted out of it. Any other directory without an entry file in it
# is not a sketch and nothing in it is compiled.
#
# An OBJECT library, not an archive: registration happens in a
# namespace-scope initializer with nothing referring to it, which an
# archive would drop. Object libraries link every object by
# construction — no -force_load, and every host shares one compile.
file(GLOB SIGIL_SKETCH_FILES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
list(REMOVE_ITEM SIGIL_SKETCH_FILES ${CMAKE_CURRENT_LIST_DIR}/Anchor.cpp)

# The directory sketches: a source one level down whose stem is the name
# of the directory it stands in is an entry, and the other sources in
# that directory are its units.
file(GLOB SIGIL_SKETCH_NESTED_FILES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_LIST_DIR}/*/*.cpp)
set(SIGIL_SKETCH_UNITS)
foreach(nested_file IN LISTS SIGIL_SKETCH_NESTED_FILES)
  get_filename_component(sketch_dir ${nested_file} DIRECTORY)
  get_filename_component(sketch_stem ${sketch_dir} NAME)
  if(sketch_stem STREQUAL "shared")
    continue()
  endif()
  if(nested_file STREQUAL "${sketch_dir}/${sketch_stem}.cpp")
    list(APPEND SIGIL_SKETCH_FILES ${nested_file})
    file(GLOB sketch_units CONFIGURE_DEPENDS ${sketch_dir}/*.cpp)
    list(REMOVE_ITEM sketch_units ${nested_file})
    list(APPEND SIGIL_SKETCH_UNITS ${sketch_units})
  endif()
endforeach()
file(GLOB SIGIL_SKETCH_SHARED_FILES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_LIST_DIR}/shared/*.cpp)

# THE SKETCHES THAT NEED A LIBRARY THIS BUILD MAY NOT HAVE: stem, then
# the target that must exist for it to compile. A sketch over an optional
# SDK is left out of the glob when its target is absent, and the target
# joins the sketch API surface when it is present — one line states both,
# because they are the same fact.
#
# A TABLE RATHER THAN A FILENAME CONVENTION. A convention would put the
# requirement in the sketch's name, where it would outlive the
# requirement and misname the piece; here the stem stays free to say what
# the sketch IS, and this file — the one place that already decides what
# is compiled — says what it needs. It is short by construction: a
# requirement is a defect in anything but a sketch that draws an SDK.
set(SIGIL_SKETCH_REQUIREMENTS
  substance_swatches SigilSubstance
  web_panel          SigilComposeWeb
  web_panel          SigilScry)

list(LENGTH SIGIL_SKETCH_REQUIREMENTS _requirement_entries)
math(EXPR _requirement_last "${_requirement_entries} / 2 - 1")
foreach(pair RANGE ${_requirement_last})
  math(EXPR _stem_index "${pair} * 2")
  math(EXPR _target_index "${pair} * 2 + 1")
  list(GET SIGIL_SKETCH_REQUIREMENTS ${_stem_index} sketch_stem)
  list(GET SIGIL_SKETCH_REQUIREMENTS ${_target_index} sketch_needs)
  if(TARGET ${sketch_needs})
    list(APPEND SIGIL_SKETCH_OPTIONAL_LIBRARIES ${sketch_needs})
  else()
    # Whichever form the sketch takes: the bare file, or the entry and
    # every unit of its directory.
    list(REMOVE_ITEM SIGIL_SKETCH_FILES
         ${CMAKE_CURRENT_LIST_DIR}/${sketch_stem}.cpp
         ${CMAKE_CURRENT_LIST_DIR}/${sketch_stem}/${sketch_stem}.cpp)
    list(FILTER SIGIL_SKETCH_UNITS EXCLUDE
         REGEX "/${sketch_stem}/[^/]+\\.cpp$")
    message(STATUS
      "sketch ${sketch_stem} left out: ${sketch_needs} is not configured")
  endif()
endforeach()
# WHICH SKETCHES THIS TREE CARRIES. Empty, the default, is every one the
# directory holds. A list of stems narrows the registry to those — for a
# pass over the libraries or the host that wants Sketchbook over the one
# sketch it is looking at while the rest are still being brought onto a
# new vocabulary. The sketches come last, and a host links every sketch
# it carries, so without this a broken sketch anywhere stands between a
# pass and the one it is studying. The directory stays the only list of
# what a sketch IS; this says only which of them one tree compiles.
set(SIGIL_SKETCH_ONLY "" CACHE STRING
  "Stems of the sketches to compile in; empty for every one")
if(SIGIL_SKETCH_ONLY)
  foreach(sketch_file IN LISTS SIGIL_SKETCH_FILES)
    get_filename_component(sketch_stem ${sketch_file} NAME_WE)
    if(NOT sketch_stem IN_LIST SIGIL_SKETCH_ONLY)
      list(REMOVE_ITEM SIGIL_SKETCH_FILES ${sketch_file})
      list(FILTER SIGIL_SKETCH_UNITS EXCLUDE
           REGEX "/${sketch_stem}/[^/]+\\.cpp$")
    endif()
  endforeach()
  message(STATUS "sketches narrowed to: ${SIGIL_SKETCH_ONLY}")
endif()
foreach(sketch_file IN LISTS SIGIL_SKETCH_FILES)
  get_filename_component(sketch_stem ${sketch_file} NAME_WE)
  set_source_files_properties(${sketch_file} PROPERTIES
    COMPILE_DEFINITIONS "SIGIL_SKETCH_STATIC=\"${sketch_stem}\"")
endforeach()

add_library(SigilSketches OBJECT
  Anchor.cpp
  ${SIGIL_SKETCH_FILES}
  ${SIGIL_SKETCH_UNITS}
  ${SIGIL_SKETCH_SHARED_FILES}
)
# THE SKETCH API SURFACE, stated once. What a sketch may #include is
# exactly this target's PUBLIC dependencies, and the flags a hot-reloaded
# sketch compiles with are lifted from Anchor.cpp's entry in the
# compilation database — which is a source of THIS target. So the header
# surface a compiled-in sketch sees and the one a hot-reloaded sketch
# sees cannot drift apart.
#
# Every library a sketch includes is named here, by its umbrella where
# the library has one and by feature target where the umbrella leaves a
# feature out (compose's texture and testing leaves; weave's
# kit and ports) or would bring a device with it (world's diligent
# feature, which only the app installs). A sketch reaches no library
# through a neighbour's link line.
target_link_libraries(SigilSketches PUBLIC
  SigilSketchCanvas SigilSketchSet
  SigilCompose SigilComposeTexture SigilComposeTesting
  SigilGeometry SigilMaterial
  SigilWeave SigilWeaveKit SigilWeavePorts
  SigilWorldElement SigilWorldFrame SigilWorldScene SigilWorldKit
  SigilCoreCompute SigilImage SigilMotion SigilLoader
  ${SIGIL_SKETCH_OPTIONAL_LIBRARIES})
# This directory is on the include path for one reason: a module under
# shared/ is spelled <shared/Name.h>, so the include names the layer.
# PUBLIC, because the flags a hot-reloaded sketch compiles with are
# lifted from this target's compile line — the same surface, once.
target_include_directories(SigilSketches PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(SigilSketches PUBLIC
  SIGIL_SKETCH_ASSET_DIR="${SIGIL_SKETCH_ASSET_DIR}"
  SIGIL_SKETCH_DIR="${SIGIL_SKETCH_DIR}"
  SIGIL_ASSET_DIR="${SIGIL_ASSET_DIR}")

# Where the Substance SDK stands, for the sketch that draws one of its
# sample archives. On the TARGET rather than on the source file, because
# a hot-reloaded sketch compiles with the flags lifted from Anchor.cpp:
# a definition only the compiled-in copy carried would break the file the
# moment someone edited it.
if(TARGET SigilSubstance)
  target_compile_definitions(SigilSketches PUBLIC
    SIGIL_SUBSTANCE_SDK_DIR="${SUBSTANCE_SDK_DIR}")
endif()
set_target_properties(SigilSketches PROPERTIES POSITION_INDEPENDENT_CODE ON)
