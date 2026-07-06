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

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 QuickLayouts CanvasPainter Network GuiPrivate)
find_package(gtest CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)

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
