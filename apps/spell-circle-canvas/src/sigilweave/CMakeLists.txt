# SigilWeave — a Skia text layout engine on HarfBuzz and ICU, as one
# feature library per directory. Each feature is its own static archive
# with its own tests and benchmarks, and links only the features beneath
# it, so a consumer that needs one tier links that tier alone.
# `SigilWeave` is the umbrella: an interface over every engine feature,
# for a consumer of the whole engine. Deliberately Qt-free below the qt
# feature: the engine consumes UTF-8/UTF-16 text plus SkTypefaces and
# produces positioned SkTextBlobs. README.md in this directory is the
# canon.
#
# Every public header lives under one include root, include/sigilweave/
# <feature>/, so a feature adds the root and spells its own directory.
# Test fixtures every binary shares sit in test/support/ and the bench
# corpus in bench/support/; a feature's test and bench binaries add those
# directories and include "support/<Name>.h".

# Library version, independent of the host application: bump when the API
# changes so an eventual vcpkg registry port can pin releases.
set(SIGILWEAVE_VERSION 0.1.0)
option(TEXTFLOW_ENABLE_INSTALL
  "Generate install/export rules so find_package(SigilWeave) works from an install tree"
  ON)

set(SIGIL_WEAVE_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
set(SIGIL_WEAVE_TEST_SUPPORT_DIR ${CMAKE_CURRENT_LIST_DIR}/test)
set(SIGIL_WEAVE_TEST_ASSET_DIR ${CMAKE_CURRENT_LIST_DIR}/test/assets)
set(SIGIL_WEAVE_BENCH_SUPPORT_DIR ${CMAKE_CURRENT_LIST_DIR}/bench)

# Dependencies point down this list.
add_subdirectory(unicode)
add_subdirectory(style)
add_subdirectory(fonts)
add_subdirectory(paragraph)
add_subdirectory(layout)
add_subdirectory(decoration)
add_subdirectory(paint)
add_subdirectory(choreograph)
add_subdirectory(query)
add_subdirectory(cache)
add_subdirectory(shaders)
add_subdirectory(ports)
add_subdirectory(kit)
add_subdirectory(qt)

# The engine: every feature a consumer of <sigilweave/SigilWeave.h>
# reaches. Shaders, ports, the kit and the Qt bridge stay separate
# targets, as content, platform glue, consumer discipline and a bridge
# rather than engine.
add_library(SigilWeave INTERFACE)
target_link_libraries(SigilWeave INTERFACE
  SigilWeaveUnicode
  SigilWeaveStyle
  SigilWeaveFonts
  SigilWeaveParagraph
  SigilWeaveLayout
  SigilWeaveDecoration
  SigilWeavePaint
  SigilWeaveChoreograph
  SigilWeaveQuery
  SigilWeaveCache
)

# Example programs: the headless PNG demo and the interactive Qt gallery.
add_subdirectory(examples/demo)
add_subdirectory(examples/gallery)

# ── Install / export (find_package(SigilWeave) from an install tree) ───────
# The eventual private vcpkg registry port drives exactly these rules; the
# generated SigilWeaveConfig.cmake re-finds the dependencies (the libraries
# are static archives, so even PRIVATE deps must be resolvable at consumer
# link time) and exposes the Qt bridge behind an optional "Qt" component:
#   find_package(SigilWeave CONFIG REQUIRED)                # engine, Qt-free
#   find_package(SigilWeave CONFIG REQUIRED COMPONENTS Kit) # + sigil::weave::SigilWeaveKit
#   find_package(SigilWeave CONFIG REQUIRED COMPONENTS Qt)  # + sigil::weave::SigilWeaveQt
if(TEXTFLOW_ENABLE_INSTALL)
  include(CMakePackageConfigHelpers)

  # SigilWeaveShaders is not exported: its paints are SigilMaterial's kit
  # recipes shaded through that library's Skia backend, and a package of
  # this engine alone cannot carry that dependency.
  install(TARGETS
      SigilWeaveUnicode SigilWeaveStyle SigilWeaveFonts SigilWeaveParagraph
      SigilWeaveLayout SigilWeaveDecoration SigilWeavePaint
      SigilWeaveChoreograph SigilWeaveQuery SigilWeaveCache SigilWeave
      SigilWeavePorts SigilWeaveKit SigilWeaveQt
    EXPORT SigilWeaveTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )
  install(DIRECTORY include/sigilweave DESTINATION include)

  install(EXPORT SigilWeaveTargets
    NAMESPACE sigil::weave::
    DESTINATION share/sigilweave
  )

  configure_package_config_file(
    cmake/SigilWeaveConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SigilWeaveConfig.cmake
    INSTALL_DESTINATION share/sigilweave
  )
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SigilWeaveConfigVersion.cmake
    VERSION ${SIGILWEAVE_VERSION}
    COMPATIBILITY SameMinorVersion
  )
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/SigilWeaveConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/SigilWeaveConfigVersion.cmake
    DESTINATION share/sigilweave
  )
endif()

# API documentation (docs/README.md).
sigil_add_docs(
  NAME SigilWeave
  BRIEF "Styled Unicode text shaped into positioned glyph runs ready for a Skia canvas"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
