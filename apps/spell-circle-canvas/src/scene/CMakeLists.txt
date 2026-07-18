# Qt-free SpellCircle scene core shared by the Qt receiver app and the
# native macOS app (src/mac): FlatBuffers scene decode into an entt
# registry, author-space -> native-canvas geometry resolution, and the
# Skia/TextFlow scene drawing implementation (including ring-label
# geometry). Anything Qt-specific — QUdpSocket transport, QML models,
# QCanvasPainter fallback drawing — stays out of this target.
add_library(SpellCircleScene STATIC
  SceneModel.cpp include/SceneModel.h
  SceneGeometry.cpp include/SceneGeometry.h
  SceneRenderer.cpp include/SceneRenderer.h
  SceneLabels.cpp include/SceneLabels.h
)

target_link_libraries(SpellCircleScene
    PUBLIC
      EnTT::EnTT
      unofficial::skia::skia
      TextFlow                # SceneLabels.h/SceneRenderer.h expose textflow types
      absl::flat_hash_map     # SceneLabels.h exposes the ring-geometry cache map
    PRIVATE
      TextFlowPorts           # system font manager for the default FontContext
      flatbuffers::flatbuffers
)

target_include_directories(SpellCircleScene PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

# SpellCircle_generated.h (regenerated from SpellCircle.fbs) lives with the
# network transport; only SceneModel.cpp reads it, so the path stays PRIVATE.
target_include_directories(SpellCircleScene PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../network/include
)
