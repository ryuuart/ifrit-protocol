# The cross-platform Qt Quick SpellCircle receiver. One directory, one
# include/ and qml/ tree, several targets kept separate so concerns stay
# layered:
#
#   SpellCircleModels   observable C++ scene and graphics state
#   SpellCircleCanvas   the canvas and its QML types (SpellCircle.Canvas)
#   Seer hosts the scene consumer, transport and settings window.
#
# QuickDialogs2 is named for the QML import in GraphicsSettingsForm.qml
# rather than a C++ link.
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 QuickLayouts
                                     QuickDialogs2 CanvasPainter
                                     GuiPrivate)
qt_standard_project_setup(REQUIRES 6.11)
find_package(EnTT CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Threads REQUIRED)

# ── Models: observable application state without a QML plugin ────────────
# The entt-backed SpellCircleModel decoded from the wire format and
# graphics configuration.
# Anonymous QML registration belongs to the canvas that exposes these types;
# headless state consumers need only the Qt object and value implementation.
qt_add_library(SpellCircleModels STATIC
  src/SpellCircleModel.cpp include/SpellCircleModel.h
  src/GraphicsConfig.cpp include/GraphicsConfig.h)
sigil_qt_target(SpellCircleModels)

target_link_libraries(SpellCircleModels
  PUBLIC
    Qt6::Gui
    SpellCircleDocument # accepted scene state and document
    EnTT::EnTT
  PRIVATE
    spdlog::spdlog
)

target_include_directories(SpellCircleModels
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ── Canvas: the scene item, Skia rendering, and texture publishing ───────
# The renderer owns its Graphite context and the shared Qt-free scene drawer.
# Without a Graphite context for the active QRhi the canvas stays empty.
qt_add_library(SpellCircleCanvas STATIC)
sigil_qt_target(SpellCircleCanvas)

foreach(receiver_qml IN ITEMS PreviewPane GraphicsSettingsForm)
  set_source_files_properties(qml/components/${receiver_qml}.qml
    PROPERTIES QT_RESOURCE_ALIAS ${receiver_qml}.qml)
endforeach()
qt_add_qml_module(SpellCircleCanvas
    URI SpellCircle.Canvas
    VERSION 1.0
    QML_FILES qml/components/PreviewPane.qml qml/components/GraphicsSettingsForm.qml
    SOURCES
      src/SpellCircle.cpp include/SpellCircle.h
      src/SpellCircleRenderer.cpp include/SpellCircleRenderer.h
      include/ModelTypes.h
)

target_link_libraries(SpellCircleCanvas
    PUBLIC
      Qt6::Quick
      Qt6::CanvasPainter
      SigilIOPublish
    PRIVATE
      spdlog::spdlog
      IfritQt IfritQtplugin Qt6::QuickControls2 Qt6::QuickLayouts Qt6::QuickDialogs2
      SigilMeasure # the renderer's record/submit frame timer
      SpellCircleModels
      SpellCircleScene # shared resolve/draw core (also used by ../mac)
      EnTT::EnTT
      Qt::GuiPrivate
)

# SigilSkiaQt is the Skia GPU plumbing under src/common/skia reached
# through a QRhi; SigilWeaveQt converts the canvas's Qt font and color values.
# SigilWeaveQt carries SigilWeave transitively;
# SigilWeavePorts provides the system font manager used by the
# render-thread FontContext.
target_link_libraries(SpellCircleCanvas PRIVATE SigilSkiaQt SigilWeaveQt
  SigilWeavePorts)

target_include_directories(SpellCircleCanvas PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)
