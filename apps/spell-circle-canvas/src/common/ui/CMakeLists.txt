# Ifrit.Ui — the Qt Quick controls the desktop tools share, with the
# native window dressing beside them. A Qt module rather than a Sigil
# library: its public surface is QML, and its one header is the window
# chrome the module registers.
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 QuickLayouts)
qt_standard_project_setup(REQUIRES 6.11)

set_source_files_properties(
  Theme.qml
  PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_library(IfritUi STATIC)
sigil_qt_target(IfritUi)

qt_add_qml_module(IfritUi
  URI Ifrit.Ui
  VERSION 1.0
  QML_FILES
    Theme.qml
    Checkerboard.qml
    DimensionSpinBoxes.qml
    FontFamilyField.qml
    FontSelector.qml
    GlassPanel.qml
    PanZoomCanvas.qml
  SOURCES
    WindowChrome.h
)

if(APPLE)
  sigil_frameworks(appkit AppKit)
  target_sources(IfritUi PRIVATE WindowChrome.mm)
  target_link_libraries(IfritUi PRIVATE ${appkit})
else()
  target_sources(IfritUi PRIVATE WindowChromeStub.cpp)
endif()

target_link_libraries(IfritUi
  PUBLIC
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickLayouts
)

# The documentation site, over the QML and the one header in this
# directory rather than an include/ root.
sigil_add_docs(
  NAME IfritUi
  BRIEF "Reusable Qt Quick components shared by the desktop tools, plus native window dressing"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/.
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/.)

# Every API name the prose spells, compiled against the headers that own
# it. This module's surface is QML, and a QML type name is not a C++ name
# the guard can see: what it checks here is the C++ header beside them and
# anything qualified the page grows later. The dependency targets the
# page names are CMake's own.
sigil_doc_probes(ui
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
  NAMESPACE ifrit::ui
  PRELUDES "<WindowChrome.h>"
  EXCLUDE "Qt6::Quick=a CMake target name, not a C++ name"
          "Qt6::QuickControls2=a CMake target name, not a C++ name"
          "Qt6::QuickLayouts=a CMake target name, not a C++ name"
  SUPPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  FLOORS 0,0,0,0,0
  LIBRARIES IfritUi)
