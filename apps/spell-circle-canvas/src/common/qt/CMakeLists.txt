# Ifrit.Qt — the Qt Quick controls the desktop tools share, with the
# native window dressing beside them. A Qt module rather than a Sigil
# library: its public surface is QML with native window and font services.
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 QuickLayouts QuickTest)
qt_standard_project_setup(REQUIRES 6.11)

set_source_files_properties(
  Theme.qml
  PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_library(IfritQt STATIC)
sigil_qt_target(IfritQt)

qt_add_qml_module(IfritQt
  URI Ifrit.Qt
  VERSION 1.0
  QML_FILES
    Theme.qml
    Panel.qml
    SectionHeading.qml
    FactRow.qml
    StatusIndicator.qml
    IconButton.qml
    SegmentedControl.qml
    SearchField.qml
    SliderField.qml
    Checkerboard.qml
    DimensionSpinBoxes.qml
    FontFamilyField.qml
    FontSelector.qml
    GlassPanel.qml
    PanZoomCanvas.qml
  SOURCES
    WindowChrome.h
    WindowCapture.h WindowCapture.cpp
    FontDatabase.h FontDatabase.cpp
    TexturePublisher.h TexturePublisher.cpp
)

if(APPLE)
  sigil_frameworks(appkit AppKit)
  target_sources(IfritQt PRIVATE WindowChrome.mm BackgroundRendering.mm)
  target_link_libraries(IfritQt PRIVATE ${appkit})
else()
  target_sources(IfritQt PRIVATE WindowChromeStub.cpp)
endif()

target_link_libraries(IfritQt
  PUBLIC
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickLayouts
    SigilPublish
  PRIVATE Qt::GuiPrivate
)

target_include_directories(IfritQt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

qt_add_executable(qt_qml_test test/ControlsTest.cpp)
sigil_qt_target(qt_qml_test)
target_link_libraries(qt_qml_test PRIVATE IfritQtplugin Qt6::QuickTest)
set_target_properties(qt_qml_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/tests")
add_test(NAME qt_controls COMMAND qt_qml_test
  -input "${CMAKE_CURRENT_SOURCE_DIR}/test/tst_controls.qml" -platform offscreen)
set_tests_properties(qt_controls PROPERTIES
  ENVIRONMENT "QT_QUICK_BACKEND=software" TIMEOUT 30)

# The documentation site, over the QML and the service headers in this
# directory rather than an include/ root.
sigil_add_docs(
  NAME IfritQt
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
sigil_doc_probes(qt
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
  NAMESPACE ifrit::qt
  PRELUDES "<WindowChrome.h>" "<WindowCapture.h>" "<TexturePublisher.h>"
  EXCLUDE "Qt6::Quick=a CMake target name, not a C++ name"
          "Qt6::QuickControls2=a CMake target name, not a C++ name"
          "Qt6::QuickLayouts=a CMake target name, not a C++ name"
  SUPPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  FLOORS 2,0,1,0,0
  LIBRARIES IfritQt)
