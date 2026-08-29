set_source_files_properties(
  Theme.qml
  PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_library(IfritUi STATIC)

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
  target_sources(IfritUi PRIVATE WindowChrome.mm)
  target_link_libraries(IfritUi PRIVATE ${APPKIT_FRAMEWORK})
else()
  target_sources(IfritUi PRIVATE WindowChromeStub.cpp)
endif()

target_link_libraries(IfritUi
  PUBLIC
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickLayouts
)

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME IfritUi
  BRIEF "Reusable Qt Quick components shared by the desktop tools, plus native window dressing"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/.
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/.)
