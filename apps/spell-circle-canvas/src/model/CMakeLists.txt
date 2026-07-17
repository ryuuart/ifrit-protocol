# Scene/application model layer: the entt-backed SpellCircleModel decoded
# from the wire format, graphics configuration, the QML-facing Models
# aggregate, and the font database. Split out of src/network so transport
# and model concerns stay separate — the network target carries datagrams;
# everything that interprets or presents them lives here.
qt_add_qml_module(SpellCircleModels
    URI SpellCircle.Models
    VERSION 1.0
    SOURCES
      SpellCircleModel.cpp include/SpellCircleModel.h
      GraphicsConfig.cpp include/GraphicsConfig.h
      Models.cpp include/Models.h
      FontDatabase.cpp include/FontDatabase.h
)

target_link_libraries(SpellCircleModels
  PUBLIC
    SpellCircleNetwork # NetworkManager + the generated wire schema
    EnTT::EnTT
  PRIVATE
    spdlog::spdlog
    flatbuffers::flatbuffers
)

target_include_directories(SpellCircleModels
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
