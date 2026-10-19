# The window a host shows a sketch in: a Qt Quick pane that draws one live
# host into its own texture, through Skia Graphite on the window's device
# where the QRhi renders with Metal and through an uploaded raster buffer
# elsewhere, behind `sketch::Window`, a door whose header names no Qt. A
# target of its own because it is the one part of the framework that links
# a window system: SigilSketch stays Qt-free, and only a process that
# shows a window links this.
find_package(Qt6 REQUIRED COMPONENTS Core Gui GuiPrivate Quick)
qt_standard_project_setup(REQUIRES 6.11)

set(SIGIL_SKETCH_WINDOW_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/CanvasView.cpp
  ${CMAKE_CURRENT_LIST_DIR}/Window.cpp
  ${CMAKE_CURRENT_LIST_DIR}/WindowCanvas.cpp
  ${CMAKE_CURRENT_LIST_DIR}/WindowCanvas.h
  ${CMAKE_CURRENT_LIST_DIR}/WindowPainter.cpp)
set(SIGIL_SKETCH_WINDOW_HEADERS
  ${SIGIL_INCLUDE_DIR}/sigilsketch/window/CanvasView.h
  ${SIGIL_INCLUDE_DIR}/sigilsketch/window/Window.h
  ${SIGIL_INCLUDE_DIR}/sigilsketch/window/WindowPainter.h)

qt_add_library(SigilSketchWindow STATIC
  ${SIGIL_SKETCH_WINDOW_SOURCES} ${SIGIL_SKETCH_WINDOW_HEADERS})
sigil_qt_target(SigilSketchWindow)
target_link_libraries(SigilSketchWindow
  PUBLIC SigilSketch SigilSkiaGraphite
  PRIVATE SigilSkiaQt SigilCoreHardware SigilGeometryDevice SigilMaterialSkia
          SigilMotionClock Qt6::Core Qt6::Gui Qt6::GuiPrivate Qt6::Quick)

# THE SAME WINDOW FOR AN ORDINARY PYTHON PROCESS, loaded by the extension
# the first time a script opens one, so importing the package loads no Qt.
# The extension already carries every framework archive, and a second copy
# here would be a second registry, a second device and a second set of
# Skia caches the extension's sessions never see; so this compiles the
# window's own units and Qt's adapter alone and resolves everything else
# out of the extension that loads it, the way a sketch library resolves
# out of its host.
if(SIGIL_BUILD_PYTHON)
  if(APPLE)
    set(sigil_window_adapter
      ${PROJECT_SOURCE_DIR}/src/common/skia/qt/QtInteropMetal.mm)
  else()
    set(sigil_window_adapter
      ${PROJECT_SOURCE_DIR}/src/common/skia/qt/QtInteropVulkan.cpp)
  endif()
  add_library(sigil_window MODULE
    ${SIGIL_SKETCH_WINDOW_SOURCES} ${sigil_window_adapter})
  sigil_qt_target(sigil_window)
  target_link_libraries(sigil_window PRIVATE
    $<COMPILE_ONLY:SigilSketchWindow> $<COMPILE_ONLY:SigilSkiaQt>
    $<COMPILE_ONLY:SigilCoreHardware> $<COMPILE_ONLY:SigilGeometryDevice>
    $<COMPILE_ONLY:SigilMaterialSkia> $<COMPILE_ONLY:SigilMotionClock>
    Qt6::Core Qt6::Gui Qt6::GuiPrivate Qt6::Quick)
  set_target_properties(sigil_window PROPERTIES
    OUTPUT_NAME _sigil_window
    PREFIX ""
    SUFFIX ".so"
    INSTALL_RPATH_USE_LINK_PATH ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python/$<0:>")
  if(APPLE)
    target_link_options(sigil_window PRIVATE "LINKER:-undefined,dynamic_lookup")
  endif()
endif()

# The cases: a window opened and closed on a host with no application
# around it. The ones that put a window on screen want a window server and
# a graphics device, and carry the label that says so.
sigil_test(sketch_test
  SOURCES test/WindowTest.cpp
  LIBRARIES SigilSketchWindow SigilWeavePorts
  SUITES SketchWindowOnScreen LABELS gpu)
