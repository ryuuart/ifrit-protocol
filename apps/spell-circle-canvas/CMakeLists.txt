cmake_minimum_required(VERSION 3.16)

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

# Builds IfritWeb (src/common/web): HTML/CSS layout rendered by the
# Ultralight SDK onto SkImage frames for the scene canvases. Auto-disables
# with a warning when the SDK isn't installed (see cmake/FindUltralight.cmake).
option(SPELLCIRCLE_ENABLE_ULTRALIGHT "Build the IfritWeb Ultralight HTML-layout backend" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 QuickLayouts CanvasPainter Network GuiPrivate)
find_package(gtest CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)
find_package(entt CONFIG REQUIRED)
find_package(benchmark CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)
# Skia is needed unconditionally now: the TextFlow text-layout library
# (src/textflow) builds on it even when the Skia scene backend is disabled.
find_package(unofficial-skia CONFIG REQUIRED)
find_package(ICU REQUIRED COMPONENTS i18n uc data)
find_package(harfbuzz CONFIG REQUIRED COMPONENTS icu)
find_package(yoga CONFIG REQUIRED)
find_package(choreograph CONFIG REQUIRED)

if(SPELLCIRCLE_ENABLE_ULTRALIGHT)
  find_package(Ultralight)
  if(NOT Ultralight_FOUND)
    message(WARNING "Ultralight SDK not found; disabling the IfritWeb backend")
    set(SPELLCIRCLE_ENABLE_ULTRALIGHT OFF)
  endif()
endif()

enable_testing()

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
