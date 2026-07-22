cmake_minimum_required(VERSION 3.16)

# Our third-party interfaces overlap heavily: skia, freetype (via harfbuzz),
# OpenImageIO and OpenColorIO each flatten libpng/libz/libexpat/Imath into
# their own link interface, so every executable named several archives twice
# and ld64 answered with `ignoring duplicate libraries` on each. Under
# cmake_minimum_required(3.16) both de-duplication policies default to OLD,
# which is what kept the repeats. ld64 records symbols as it scans and
# re-visits earlier archives, so one occurrence is enough.
#
# These must be set BEFORE project(): CMP0156's NEW behavior is carried by
# CMAKE_<LANG>_LINK_LIBRARIES_PROCESSING, which the platform modules
# initialize while enabling the language.
if(POLICY CMP0156)
  cmake_policy(SET CMP0156 NEW) # de-duplicate by linker capability
endif()
if(POLICY CMP0179)
  cmake_policy(SET CMP0179 NEW) # ...keeping the first occurrence, everywhere
endif()

project(SpellCircleCanvas VERSION 0.1 LANGUAGES CXX)

set(QT_QML_GENERATE_QMLLS_INI ON) # QMLLS Language Server config
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Redirects final executables (.exe, ELF binaries)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Redirects dynamically linked libraries (.so, .dll)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Redirects static libraries / archive files (.a, .lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Redirects QT modules
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

# Compiles the Skia Graphite CanvasSceneBackend (Metal on Apple platforms,
# Vulkan elsewhere) and its generic GPU interop plumbing under
# src/common/skia. At runtime the renderer uses it when a compatible
# Graphite context can be created; without it the canvas renders no scene
# content. Turning this off removes the Graphite implementation and
# leaves the same renderer-facing factory backed by its always-null stub.
option(SPELLCIRCLE_ENABLE_SKIA_CANVAS "Build the Skia Graphite (Metal/Vulkan) offscreen-canvas backend" ON)

# Builds SigilScry (src/common/scry): HTML/CSS layout rendered by the
# Ultralight SDK onto SkImage frames for the scene canvases. Auto-disables
# with a warning when the SDK isn't installed (see cmake/FindUltralight.cmake).
option(SPELLCIRCLE_ENABLE_ULTRALIGHT "Build the SigilScry Ultralight HTML-layout backend" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 QuickLayouts CanvasPainter Network GuiPrivate)
find_package(gtest CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)
find_package(entt CONFIG REQUIRED)
find_package(benchmark CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)
# Skia is needed unconditionally now: the SigilWeave text-layout library
# (src/sigilweave) builds on it even when the Skia scene backend is disabled.
find_package(unofficial-skia CONFIG REQUIRED)
find_package(ICU REQUIRED COMPONENTS i18n uc data)
find_package(harfbuzz CONFIG REQUIRED COMPONENTS icu)
find_package(yoga CONFIG REQUIRED)
find_package(choreograph CONFIG REQUIRED)

if(SPELLCIRCLE_ENABLE_ULTRALIGHT)
  find_package(Ultralight)
  if(NOT Ultralight_FOUND)
    message(WARNING "Ultralight SDK not found; disabling the SigilScry backend")
    set(SPELLCIRCLE_ENABLE_ULTRALIGHT OFF)
  endif()
endif()

enable_testing()

# What the policies above cannot reach: vcpkg's exported configs disagree
# about how to name the same archive. freetype pulls libpng and zlib through
# the PNG::PNG / ZLIB::ZLIB imported targets, while unofficial-skia flattens
# its whole closure into raw find_library() paths inside one
# $<LINK_ONLY:$<$<CONFIG:DEBUG>:...>> blob. CMake de-duplicates targets
# against targets and paths against paths, never a target against a path, so
# libpng/libz/libexpat/libjpeg still reach ld64 twice. The second copy is
# ignored by the linker, which is correct but noisy — this silences the note
# without pretending the link line is minimal.
if(APPLE AND NOT CMAKE_VERSION VERSION_LESS 3.18) # CheckLinkerFlag
  include(CheckLinkerFlag)
  check_linker_flag(CXX "-Wl,-no_warn_duplicate_libraries"
    HAVE_NO_WARN_DUPLICATE_LIBRARIES) # ld64 from Xcode 15 on
  if(HAVE_NO_WARN_DUPLICATE_LIBRARIES)
    add_link_options("LINKER:-no_warn_duplicate_libraries")
  endif()
endif()

# MacOS Frameworks
if(APPLE)
  find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
  find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
  find_library(CORE_GRAPHICS_FRAMEWORK CoreGraphics)
  find_library(SYPHON_FRAMEWORK Syphon)
endif()

qt_standard_project_setup(REQUIRES 6.11)

add_subdirectory(src)

include(GNUInstallDirs)
install(TARGETS SpellCircle
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
