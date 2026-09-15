# Seer — the one window over the wires.
#
# The C++ here is view-models and nothing else: two list models, two
# objects a pane binds to, and one session that owns the library's values
# and drives them from a timer. Every claim about what a wire DOES is
# asserted in the library's own test binary, which has no toolkit in it;
# what is left here is which pane shows what.
#
# The controls are Ifrit.Ui's — the theme derived from the system
# palette, the glass panel, the native window dressing — so this window
# follows the light and dark appearance of the machine and invents no
# look of its own.
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
    Main ConnectionsPane ReceivePane SendPane RecordStrip)
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
  SOURCES
    SeerSession.cpp SeerSession.h
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
  SigilSeerWire SigilDataDecode
  IfritUiplugin Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::QuickLayouts
  Qt6::QuickDialogs2)
