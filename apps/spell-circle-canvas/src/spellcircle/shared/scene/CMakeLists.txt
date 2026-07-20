# Qt-free SpellCircle scene core shared by the Qt receiver app
# (../../qt) and the native macOS app (../../mac): FlatBuffers scene
# decode into an entt registry, author-space -> native-canvas geometry
# resolution, and the Skia/SigilWeave scene drawing implementation
# (including ring-label geometry). Anything Qt-specific — transport
# adapters, QML models — stays out of this target.
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
      SigilWeave                # SceneLabels.h/SceneRenderer.h expose sigil::weave types
      absl::flat_hash_map     # SceneLabels.h exposes the ring-geometry cache map
    PRIVATE
      SigilWeavePorts           # system font manager for the default FontContext
      # Only SceneModel.cpp reads the generated wire schema, so the
      # dependency stays PRIVATE.
      SpellCircleSchema
)

target_include_directories(SpellCircleScene PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)
