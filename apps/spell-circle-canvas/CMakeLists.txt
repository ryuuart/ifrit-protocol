# The floor is set by the policies this build depends on being NEW: link
# de-duplication by linker capability and its first-occurrence rule, and
# a GENERATED source property visible in every directory — which is what
# lets the wire schema's generated header be compiled in directories
# other than the one that generates it.
cmake_minimum_required(VERSION 3.31)

project(SpellCircleCanvas VERSION 0.1 LANGUAGES CXX)

set(QT_QML_GENERATE_QMLLS_INI ON) # QMLLS Language Server config
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# A package found in two directories otherwise defines its imported
# targets twice — once per directory scope — and the same archive reaches
# ld64 under both names, since CMake collapses two link items only when
# they are one item. Promoting them makes the second find_package() see
# the targets the first defined.
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

# Builds SigilScry (src/common/scry): HTML/CSS layout rendered by the
# Ultralight SDK onto SkImage frames for the scene canvases. Auto-disables
# with a warning when the SDK isn't installed.
option(SPELLCIRCLE_ENABLE_ULTRALIGHT "Build the SigilScry Ultralight HTML-layout backend" ON)

# Builds SigilSubstance (src/common/substance): Adobe Substance 3D
# archives (.sbsar) rendered to texture sets through the Substance
# Engine, for SigilWorld materials. The SDK is a licensed download and is
# never vendored; scripts/setup.py finds an installed copy and writes
# SUBSTANCE_SDK_DIR into CMakeUserPresets.json. Auto-disables with a
# warning when the SDK is not there.
option(SPELLCIRCLE_ENABLE_SUBSTANCE "Build the SigilSubstance .sbsar material renderer" ON)
# Builds SigilUsd (src/common/usd): the world's data written to and read
# from USD (OpenUSD core from vcpkg — tbb and zlib only). Auto-disables
# with a warning when the pxr package is not found.
option(SPELLCIRCLE_ENABLE_USD "Build the SigilUsd writer/reader over OpenUSD" ON)
set(SUBSTANCE_SDK_DIR "" CACHE PATH "Adobe Substance 3D SDK directory (holds substance-config.cmake)")

# workaround: vcpkg's toolchain orders its own two prefixes — the release
# tree and debug/ beside it — by CMAKE_BUILD_TYPE, which a multi-config
# generator never sets, and reads "not set" as Debug: the debug prefix
# lands first, and every find_library() that resolves one archive for all
# configurations answers with the debug one. A Release binary then carries
# debug archives (skia's dav1d, curl's ssl and crypto, ffmpeg's vpx and
# openh264 among them). The prefixes are re-ordered here, before the first
# find_package(), so the release tree is searched first; a package whose
# module resolves a release and a debug archive as a pair is unaffected,
# since it names each by its own variable.
foreach(_prefix_list IN ITEMS CMAKE_PREFIX_PATH CMAKE_LIBRARY_PATH
                              CMAKE_FIND_ROOT_PATH)
  set(_suffix "")
  if(_prefix_list STREQUAL CMAKE_LIBRARY_PATH)
    set(_suffix "/lib/manual-link")
  endif()
  set(_release "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}${_suffix}")
  set(_debug "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug${_suffix}")
  set(_paths "${${_prefix_list}}")
  list(REMOVE_ITEM _paths "${_release}" "${_debug}")
  list(INSERT _paths 0 "${_release}" "${_debug}")
  set(${_prefix_list} "${_paths}")
endforeach()
unset(_prefix_list)
unset(_suffix)
unset(_release)
unset(_debug)
unset(_paths)

# cmake/ holds the modules the whole tree is built from — the library
# helper, the instrumentation switch, the Qt target rule, the header
# self-test. A module that belongs to ONE library lives in that library's
# own cmake/ directory and is included by its path, so moving the library
# moves its build rules with it.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# workaround: Qt ships a FindFFmpeg module that collides with the ffmpeg
# port's FindFFMPEG on a case-insensitive filesystem. The port's module —
# which is what resolves the release and debug archives as a pair — is
# reached by asking for the package before Qt appends its own module
# directory to the search path.
find_package(FFMPEG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 QuickLayouts QuickDialogs2 CanvasPainter Network GuiPrivate)
find_package(gtest CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)
find_package(entt CONFIG REQUIRED)
find_package(benchmark CONFIG REQUIRED)
find_package(boost_container CONFIG REQUIRED)
find_package(boost_container_hash CONFIG REQUIRED)
find_package(boost_unordered CONFIG REQUIRED)
find_package(unofficial-skia CONFIG REQUIRED)
find_package(ICU REQUIRED COMPONENTS i18n uc data)
find_package(harfbuzz CONFIG REQUIRED COMPONENTS icu)
find_package(yoga CONFIG REQUIRED)
find_package(choreograph CONFIG REQUIRED)

# The administrative steps around the build — extracting the sketch
# compile flags, fetching assets, generating the API documentation — are
# Python scripts under scripts/, and the build calls them rather than
# re-implementing text and file work in CMake.
find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(SPELLCIRCLE_ENABLE_ULTRALIGHT)
  # SigilScry carries the find module for the SDK it is the only consumer of.
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/src/common/scry/cmake")
  find_package(Ultralight)
  if(NOT Ultralight_FOUND)
    message(WARNING "Ultralight SDK not found; disabling the SigilScry backend")
    set(SPELLCIRCLE_ENABLE_ULTRALIGHT OFF)
  endif()
endif()

if(SPELLCIRCLE_ENABLE_USD)
  find_package(pxr CONFIG QUIET)
  if(NOT pxr_FOUND)
    message(WARNING "OpenUSD (pxr) not found; disabling SigilUsd")
    set(SPELLCIRCLE_ENABLE_USD OFF)
  endif()
endif()

if(SPELLCIRCLE_ENABLE_SUBSTANCE)
  if(SUBSTANCE_SDK_DIR AND EXISTS "${SUBSTANCE_SDK_DIR}/substance-config.cmake")
    # The CPU engine ("blend": results in system memory), matched to the
    # host: NEON on Apple Silicon, SSE2 on x86-64. Headless, no GPU
    # context to own, and its outputs land as bytes SigilWorld uploads
    # like any other image.
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
      set(SUBSTANCE_FRAMEWORK_ENGINE_VARIANT neon_blend)
    else()
      set(SUBSTANCE_FRAMEWORK_ENGINE_VARIANT sse2_blend)
    endif()
    include("${SUBSTANCE_SDK_DIR}/substance-config.cmake")
    message(STATUS "Substance SDK: ${SUBSTANCE_SDK_DIR} (${SUBSTANCE_FRAMEWORK_ENGINE_VARIANT})")
  else()
    message(WARNING "Substance SDK not found (SUBSTANCE_SDK_DIR unset or invalid); disabling SigilSubstance")
    set(SPELLCIRCLE_ENABLE_SUBSTANCE OFF)
  endif()
endif()

# SPELLCIRCLE_COVERAGE and SPELLCIRCLE_SANITIZE, each for a build tree of
# its own.
include(Instrumentation)

# Opt-in network fetch of open-licensed demo assets into the build tree;
# nothing here runs during a normal build, and configuring the project
# never touches the network. The manifest and the downloader are
# scripts/fetch_assets.py, which needs no build tree of its own — this
# target is the same fetch reached through the build.
set(SIGIL_ASSET_DIR "${CMAKE_BINARY_DIR}/assets" CACHE PATH
    "Where fetch_assets writes downloaded demo assets")
add_custom_target(fetch_assets
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/scripts/fetch_assets.py
          --out ${SIGIL_ASSET_DIR}
  COMMENT "Fetching demo assets into ${SIGIL_ASSET_DIR}"
  VERBATIM)

# Per-library Doxygen sites (`--target docs`); nothing here runs during a
# normal build, and the target is simply absent when Doxygen is not
# installed. Each library root registers itself; sigil_finalize_docs()
# after add_subdirectory(src) writes the rules.
include("${CMAKE_CURRENT_LIST_DIR}/docs/Docs.cmake")

# sigil_library_root(), sigil_library(), sigil_test() and sigil_bench():
# the calls every library under src/ is built from.
include(Sigil)

enable_testing()

# workaround: vcpkg configs name some dependencies as imported targets and
# others as archive paths, and one archive named both ways is two link
# items CMake cannot merge, so it reaches ld64 twice and is noted there.
if(APPLE)
  include(CheckLinkerFlag)
  check_linker_flag(CXX "-Wl,-no_warn_duplicate_libraries"
    HAVE_NO_WARN_DUPLICATE_LIBRARIES) # ld64 from Xcode 15 on
  if(HAVE_NO_WARN_DUPLICATE_LIBRARIES)
    add_link_options("LINKER:-no_warn_duplicate_libraries")
  endif()
endif()

# The macOS frameworks, found once for every target that names one.
if(APPLE)
  find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
  find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
  find_library(METAL_FRAMEWORK Metal REQUIRED)
  find_library(CORE_GRAPHICS_FRAMEWORK CoreGraphics)
endif()

# Syphon publishes a canvas as a GPU texture other applications attach to.
# Absent, the targets that publish one skip themselves.
find_package(unofficial-syphon CONFIG)

# Qt's source scanning is decided here rather than left to
# qt_standard_project_setup(), which turns moc on for every target in the
# tree unless the answer is already set. Almost nothing here is a Qt
# target, and moc costs a scan of every source plus an autogen build step
# on each target it is enabled on, found Q_OBJECT or not. So: off by
# default, on per target through sigil_qt_target(). Everything else the
# call arranges — the Qt policy level, GNUInstallDirs, IDE folders, the
# generated-source groups — is still wanted.
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
include(QtTarget)

qt_standard_project_setup(REQUIRES 6.11)

# Every benchmark binary hangs off this target, so one build command
# produces the set scripts/bench_ledger.py runs. Benchmarks are not
# tests: none is registered with ctest.
add_custom_target(benches)

add_subdirectory(src)

# Every library root has registered by now, which is what lets each
# library's Doxyfile name all the others in TAGFILES.
sigil_finalize_docs()

include(GNUInstallDirs)
install(TARGETS SpellCircle
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
