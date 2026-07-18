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
