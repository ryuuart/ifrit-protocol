# The cross-platform Qt Quick SpellCircle receiver. One directory, one
# include/ and qml/ tree, several targets kept separate so concerns stay
# layered:
#
#   SpellCircleNetwork  Qt adapter over the shared ASIO transport
#   SpellCircleModels   QML-facing models (SpellCircle.Models)
#   SpellCircleCanvas   the canvas item + Skia backend (SpellCircle.Canvas)
#   SpellCircle         the executable + application QML (SpellCircle.App)

# ── Network: the Qt face of the shared transport ─────────────────────────
# Wire transport only — marshals SpellCircleNet datagrams onto the Qt main
# thread and verifies them against the shared schema. Everything that
# interprets or presents them lives in the model layer below.
qt_add_library(SpellCircleNetwork SHARED
  src/NetworkManager.cpp include/NetworkManager.h
)
sigil_qt_target(SpellCircleNetwork)

target_link_libraries(SpellCircleNetwork
    PUBLIC
      Qt6::Network
      Qt6::Gui
    PRIVATE
      spdlog::spdlog
      SpellCircleSchema
      SpellCircleNet
)

target_include_directories(SpellCircleNetwork
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ── Models: the QML-facing application state ─────────────────────────────
# The entt-backed SpellCircleModel decoded from the wire format, graphics
# configuration, the QML-facing Models aggregate, and the font database.
#
# The library is spelled out rather than left to qt_add_qml_module() to
# create, here and for the canvas below, because a QML module registers
# its types out of moc's JSON output: the scan has to be on for the
# backing target before the module is declared.
qt_add_library(SpellCircleModels SHARED)
sigil_qt_target(SpellCircleModels)

qt_add_qml_module(SpellCircleModels
    URI SpellCircle.Models
    VERSION 1.0
    SOURCES
      src/SpellCircleModel.cpp include/SpellCircleModel.h
      src/GraphicsConfig.cpp include/GraphicsConfig.h
      src/Models.cpp include/Models.h
      src/FontDatabase.cpp include/FontDatabase.h
)

target_link_libraries(SpellCircleModels
  PUBLIC
    SpellCircleNetwork # NetworkManager transport
    SpellCircleScene # Qt-free scene document (decode + entt registry)
    EnTT::EnTT
  PRIVATE
    spdlog::spdlog
    SpellCircleSchema
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

qt_add_library(SpellCircleCanvas SHARED)
sigil_qt_target(SpellCircleCanvas)

qt_add_qml_module(SpellCircleCanvas
    URI SpellCircle.Canvas
    VERSION 1.0
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
      SigilMeasure # the record/submit frame timer in SkiaSceneBackend.cpp
      SpellCircleModels # SpellCircleModel/GraphicsConfig (carries network)
      SpellCircleScene # shared resolve/draw core (also used by ../mac)
      EnTT::EnTT
      Qt::GuiPrivate
)

if(APPLE)
  target_link_libraries(SpellCircleCanvas
      PRIVATE
        ${FOUNDATION_FRAMEWORK}
        unofficial::syphon::syphon
        ${CORE_GRAPHICS_FRAMEWORK}
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

# ── The executable ───────────────────────────────────────────────────────
qt_add_executable(SpellCircle
    src/main.cpp
)
sigil_qt_target(SpellCircle)

# App Nap suppression is a main.cpp concern, not a library: fold the one
# ObjC++ TU straight into the executable on macOS.
if(APPLE)
  target_sources(SpellCircle PRIVATE src/AppNap.mm include/AppNap.h)
  target_link_libraries(SpellCircle PRIVATE
    ${FOUNDATION_FRAMEWORK}
    ${APPKIT_FRAMEWORK}
  )
endif()

target_include_directories(SpellCircle PRIVATE include)

# Keep application QML organized by concern while exposing its types at the
# executable module root. Reusable controls live in Ifrit.Ui (src/common/ui).
set_source_files_properties(
    qml/Main.qml
    PROPERTIES QT_RESOURCE_ALIAS Main.qml
)
set_source_files_properties(
    qml/SettingsWindow.qml
    PROPERTIES QT_RESOURCE_ALIAS SettingsWindow.qml
)
set_source_files_properties(
    qml/components/LogSidebar.qml
    PROPERTIES QT_RESOURCE_ALIAS LogSidebar.qml
)
set_source_files_properties(
    qml/components/PreviewPane.qml
    PROPERTIES QT_RESOURCE_ALIAS PreviewPane.qml
)
set_source_files_properties(
    qml/components/GraphicsSettingsForm.qml
    PROPERTIES QT_RESOURCE_ALIAS GraphicsSettingsForm.qml
)
set_source_files_properties(
    qml/components/NetworkSettingsForm.qml
    PROPERTIES QT_RESOURCE_ALIAS NetworkSettingsForm.qml
)

qt_add_qml_module(SpellCircle
    URI SpellCircle.App
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/SettingsWindow.qml
        qml/components/LogSidebar.qml
        qml/components/PreviewPane.qml
        qml/components/GraphicsSettingsForm.qml
        qml/components/NetworkSettingsForm.qml
)

set_target_properties(SpellCircle PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.sigil.SpellCircle"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    # WIN32_EXECUTABLE TRUE
)

target_link_libraries(SpellCircle
    PRIVATE
      Qt6::Quick
      Qt6::QuickControls2
      Qt6::QuickLayouts
      Qt6::CanvasPainter
      spdlog::spdlog
      SpellCircleCanvas
      SpellCircleModels
)
