cmake_minimum_required(VERSION 3.16)

project(qtquick1 VERSION 0.1 LANGUAGES CXX)

set(QT_QML_GENERATE_QMLLS_INI ON) # QMLLS Language Server config
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Redirects final executables (.exe, ELF binaries)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Redirects dynamically linked libraries (.so, .dll)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Redirects static libraries / archive files (.a, .lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Redirects QT modules
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

# Whether SpellCircleRenderer is built with the Skia Graphite (Metal)
# offscreen-canvas backend compiled in — a build-time swap between two
# implementations of the same CanvasSceneBackend interface (see
# src/qml/SpellCircle/include/CanvasSceneBackend.h and
# src/qml/SpellCircle/CMakeLists.txt), not a user-facing runtime setting.
# The generic Skia/Metal GPU interop plumbing lives under
# src/platform/skia/, only added when this is ON. When ON,
# SpellCircleRenderer always draws through the Skia backend in place of
# QCanvasPainter (right now that means a handful of proof-of-concept
# circles, not the real scene — this is a development flag for the Skia
# backend itself, hence defaulting OFF).
option(SPELLCIRCLE_ENABLE_SKIA_CANVAS "Build the Skia Graphite (Metal) offscreen-canvas backend" ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 QuickLayouts CanvasPainter Network GuiPrivate)
find_package(gtest CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)
find_package(entt CONFIG REQUIRED)
if(SPELLCIRCLE_ENABLE_SKIA_CANVAS)
  find_package(unofficial-skia CONFIG REQUIRED)
endif()
find_package(icu REQUIRED COMPONENTS i18n)
find_package(harfbuzz CONFIG REQUIRED COMPONENTS icu)

# MacOS Frameworks
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(CORE_GRAPHICS_FRAMEWORK CoreGraphics)
find_library(SYPHON_FRAMEWORK Syphon)

qt_standard_project_setup(REQUIRES 6.11)

add_subdirectory(src)

include(GNUInstallDirs)
install(TARGETS SpellCircle
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
