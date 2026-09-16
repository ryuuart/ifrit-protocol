# Verified document ingestion is independent of graphics. Scene rendering
# consumes that document and owns resolution, drawing, and ring labels.
find_package(EnTT CONFIG REQUIRED)
find_package(boost_unordered CONFIG REQUIRED)
find_package(unofficial-skia CONFIG REQUIRED)
add_library(SpellCircleDocument STATIC
  SceneModel.cpp include/SceneModel.h
  SceneSession.cpp include/SceneSession.h)
target_include_directories(SpellCircleDocument PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(SpellCircleDocument
  PUBLIC EnTT::EnTT
  PRIVATE Boost::unordered SpellCircleSchema)

add_library(SpellCircleScene STATIC
  SceneGeometry.cpp include/SceneGeometry.h
  SceneRenderer.cpp include/SceneRenderer.h
  SceneLabels.cpp include/SceneLabels.h
)

target_link_libraries(SpellCircleScene
    PUBLIC
      SpellCircleDocument
      unofficial::skia::skia
      SigilWeave                # SceneLabels.h/SceneRenderer.h expose sigil::weave types
      SigilGeometryPath         # SceneLabels.h holds each ring as a geometry::path::Contour
      Boost::unordered           # SceneLabels.h exposes the ring-geometry cache map
    PRIVATE
      SigilWeavePorts           # system font manager for the default FontContext
)

sigil_test(spellcircle_document_test
  SOURCES test/SceneSessionTest.cpp
  LIBRARIES SpellCircleDocument SpellCircleSchema)

target_include_directories(SpellCircleScene PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

# The test builds wire payloads with the generated FlatBuffers API so it can
# exercise the real verify -> decode -> resolve path. Wire-schema imports
# belong to each consumer that constructs packets.
sigil_test(spellcircle_test
  SOURCES test/SceneTest.cpp
  LIBRARIES SpellCircleScene SpellCircleSchema)

sigil_doc_probes(spellcircle
  NAMESPACE spellcircle
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/../../../../README.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/io/include/sigilio/hub
           ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/io/include/sigilio/transport
  PRELUDES "\"SceneModel.h\"" "\"SceneSession.h\""
           "\"SceneRenderer.h\"" "<sigilio/hub/Hub.h>"
           "<sigilio/hub/Feed.h>" "<sigilio/transport/Transport.h>"
           "<iostream>"
  FLOORS 8,3,5,0,0
  LIBRARIES SpellCircleScene SigilIOHub SigilIOTransport)
