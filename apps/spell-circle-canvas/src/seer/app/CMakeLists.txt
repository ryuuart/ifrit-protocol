# Seer — the one window over the wires.
#
# The wire library stays toolkit-free. This application owns its view models
# and one scene consumer over the same feeds, using the shared receiver canvas.
# Ifrit.Qt supplies the palette, controls and native window dressing.
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 QuickLayouts
                                     QuickDialogs2)
qt_standard_project_setup(REQUIRES 6.11)

qt_add_executable(Seer main.cpp)
sigil_qt_target(Seer)

# A real .app: a bare Mach-O in bin/ gets no bundle identity, so macOS
# gives it the executable's name in the menu bar and treats activation as
# a plain process.
set_target_properties(Seer PROPERTIES
  MACOSX_BUNDLE_GUI_IDENTIFIER "dev.sigil.Seer"
  MACOSX_BUNDLE_BUNDLE_NAME "Seer"
  MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
  MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
  MACOSX_BUNDLE TRUE
)

# A file kept in qml/ is imported by NAME, so it is aliased to the module
# root beside the generated qmldir. The property is read when the module
# is declared, so it is set before it.
foreach(seer_qml IN ITEMS
    Main ConnectionsPane ReceivePane SendPane RecordStrip ReceiverPane ReceiverSettings)
  set_source_files_properties(qml/${seer_qml}.qml
    PROPERTIES QT_RESOURCE_ALIAS ${seer_qml}.qml)
endforeach()

qt_add_qml_module(Seer
  URI Sigil.Seer
  VERSION 1.0
  QML_FILES
    qml/Main.qml
    qml/ConnectionsPane.qml
    qml/ReceivePane.qml
    qml/SendPane.qml
    qml/RecordStrip.qml
    qml/ReceiverPane.qml
    qml/ReceiverSettings.qml
  SOURCES
    SeerSession.cpp SeerSession.h
    Receiver.cpp Receiver.h
    WireList.cpp WireList.h
    MessageList.cpp MessageList.h
    WireDetail.cpp WireDetail.h
    SendForm.cpp SendForm.h
)

# SigilData is named here and not taken from the archive's link: the
# session makes the schema it hands the wires out of a file's bytes, so
# this target spells that vocabulary itself.
target_include_directories(Seer PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(Seer PRIVATE
  SigilSeerWire SigilDataDecode SpellCircleModels SpellCircleCanvas
  SpellCircleCanvasplugin
  IfritQt IfritQtplugin Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::QuickLayouts
  Qt6::QuickDialogs2)


sigil_test(seer_qt_test
  SOURCES SeerSession.cpp Receiver.cpp WireList.cpp MessageList.cpp
          WireDetail.cpp SendForm.cpp test/ReceiverTest.cpp
  LIBRARIES SigilSeerWire SigilDataDecode SpellCircleModels SpellCircleSchema
            Qt6::Gui Qt6::Qml)
sigil_qt_target(seer_qt_test)
target_include_directories(seer_qt_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
