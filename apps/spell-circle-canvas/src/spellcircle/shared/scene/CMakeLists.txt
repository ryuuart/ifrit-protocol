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
      SigilGeometryPath         # SceneLabels.h holds each ring as a geometry::path::Contour
      Boost::unordered           # SceneLabels.h exposes the ring-geometry cache map
    PRIVATE
      SigilWeavePorts           # system font manager for the default FontContext
      # Only SceneModel.cpp reads the generated wire schema, so the
      # dependency stays PRIVATE.
      SpellCircleSchema
)

target_include_directories(SpellCircleScene PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

# The test builds wire payloads with the generated FlatBuffers API so it can
# exercise the real verify -> decode -> resolve path, which is why it links
# the schema directly instead of relying on SpellCircleScene's PRIVATE edge.
sigil_test(spellcircle_test
  SOURCES test/SceneTest.cpp
  LIBRARIES SpellCircleScene SpellCircleSchema)

# The test observes box geometry through an SkCanvas subclass, and Skia is
# built without RTTI — a derived class references its base's typeinfo, so
# this TU must match or the link fails on typeinfo for SkCanvas.
target_compile_options(spellcircle_test PRIVATE -fno-rtti)
