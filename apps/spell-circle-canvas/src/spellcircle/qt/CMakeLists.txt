# The cross-platform Qt Quick SpellCircle receiver. One directory, one
# include/ and qml/ tree, several targets kept separate so concerns stay
# layered:
#
#   SpellCircleModels   QML-facing models (SpellCircle.Models)
#   SpellCircleCanvas   the canvas item + Skia backend (SpellCircle.Canvas)
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

# ── Models: the QML-facing application state ─────────────────────────────
# The entt-backed SpellCircleModel decoded from the wire format and
# graphics configuration.
#
# The library is spelled out rather than left to qt_add_qml_module() to
# create, here and for the canvas below, because a QML module registers
# its types out of moc's JSON output: the scan has to be on for the
# backing target before the module is declared.
qt_add_library(SpellCircleModels STATIC)
sigil_qt_target(SpellCircleModels)

qt_add_qml_module(SpellCircleModels
    URI SpellCircle.Models
    VERSION 1.0
    SOURCES
      include/ModelTypes.h
      src/SpellCircleModel.cpp include/SpellCircleModel.h
      src/GraphicsConfig.cpp include/GraphicsConfig.h
)

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

# ── Canvas: the scene item, Skia backend, and texture publishing ─────────
# SkiaSceneBackend.cpp is the Qt frame around the scene drawing, which
# itself lives in the shared Qt-free ../shared/scene/SceneRenderer.cpp; the
# createSkiaSceneBackend() factory declared in include/SkiaSceneBackend.h
# answers null when the active QRhi backend has no Graphite context, and
# SpellCircleRenderer never branches on more than that null.
#
# Per-platform TexturePublisher implementation behind the one factory in
# TexturePublisherFactory.cpp: Syphon on macOS/Metal, Spout on
# Windows/D3D11 (only when vcpkg spout2[dx] is present), nothing elsewhere
# (the factory then always returns null).
set(SPELLCIRCLE_PUBLISHER_SOURCES)
if(APPLE)
  set(SPELLCIRCLE_PUBLISHER_SOURCES src/SyphonBridge.mm include/SyphonBridge.h)
elseif(WIN32)
  find_package(Spout2 CONFIG QUIET)
  if(TARGET Spout2::SpoutDX)
    set(SPELLCIRCLE_PUBLISHER_SOURCES src/SpoutBridge.cpp include/SpoutBridge.h)
  endif()
endif()

qt_add_library(SpellCircleCanvas STATIC)
sigil_qt_target(SpellCircleCanvas)

foreach(receiver_qml IN ITEMS PreviewPane GraphicsSettingsForm)
  set_source_files_properties(qml/components/${receiver_qml}.qml
    PROPERTIES QT_RESOURCE_ALIAS ${receiver_qml}.qml)
endforeach()
qt_add_qml_module(SpellCircleCanvas
    URI SpellCircle.Canvas
    VERSION 1.0
    IMPORTS SpellCircle.Models
    QML_FILES qml/components/PreviewPane.qml qml/components/GraphicsSettingsForm.qml
    SOURCES
      src/TexturePublisherFactory.cpp include/TexturePublisher.h
      ${SPELLCIRCLE_PUBLISHER_SOURCES}
      src/SpellCircle.cpp include/SpellCircle.h
      src/SpellCircleRenderer.cpp include/SpellCircleRenderer.h
      include/CanvasSceneBackend.h
      include/SkiaSceneBackend.h
      src/SkiaSceneBackend.cpp
)

target_link_libraries(SpellCircleCanvas
    PUBLIC
      Qt6::Quick
      Qt6::CanvasPainter
    PRIVATE
      spdlog::spdlog
      IfritQtplugin Qt6::QuickControls2 Qt6::QuickLayouts Qt6::QuickDialogs2
      SigilMeasure # the record/submit frame timer in SkiaSceneBackend.cpp
      SpellCircleModels
      SpellCircleScene # shared resolve/draw core (also used by ../mac)
      EnTT::EnTT
      Qt::GuiPrivate
)

if(APPLE)
  sigil_frameworks(canvas_frameworks Foundation CoreGraphics)
  find_package(unofficial-syphon CONFIG REQUIRED)
  target_link_libraries(SpellCircleCanvas
      PRIVATE
        ${canvas_frameworks}
        unofficial::syphon::syphon
  )
elseif(TARGET Spout2::SpoutDX)
  target_link_libraries(SpellCircleCanvas PRIVATE Spout2::SpoutDX d3d11)
  target_compile_definitions(SpellCircleCanvas PRIVATE SPELLCIRCLE_HAVE_SPOUT)
endif()

# SigilSkiaQt is the Skia GPU plumbing under src/common/skia reached
# through a QRhi; SigilWeave provides the animated text layer drawn by
# SkiaSceneBackend.cpp. SigilWeaveQt carries SigilWeave transitively;
# SigilWeavePorts provides the system font manager used by the
# render-thread FontContext.
target_link_libraries(SpellCircleCanvas PRIVATE SigilSkiaQt SigilWeaveQt
  SigilWeavePorts)

target_include_directories(SpellCircleCanvas PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)
