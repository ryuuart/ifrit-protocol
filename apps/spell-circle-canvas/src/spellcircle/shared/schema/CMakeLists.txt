# The FlatBuffers wire schema (SpellCircle.fbs) and its committed generated
# header, shared by every decoder: the Qt-free SceneModel, the Qt
# NetworkManager verifier, and the Python authoring package (whose modules
# regenerate alongside the header).
#
# SpellCircle_generated.h is regenerated manually from the schema — run
# scripts/regen_flatbuffers.sh from the repository root after editing
# SpellCircle.fbs, then commit the regenerated files.
add_library(SpellCircleSchema INTERFACE)

target_include_directories(SpellCircleSchema INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(SpellCircleSchema INTERFACE flatbuffers::flatbuffers)
