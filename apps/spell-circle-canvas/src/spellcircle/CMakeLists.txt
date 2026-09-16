# SpellCircle — scene receiver libraries shared by Seer and the native app:
#
#   shared/  Qt-free code both frontends embed: the wire schema and the
#            scene core (decode/resolve/draw). The port each frontend
#            listens on is a door it opens on a resource hub of its own.
#   qt/      The reusable Qt Quick receiver canvas and models hosted by Seer.
#   mac/     The native SwiftUI receiver (executable SpellCircleMac,
#            macOS + Swift toolchain only).
add_subdirectory(shared/schema)
add_subdirectory(shared/scene)
add_subdirectory(qt)
if(APPLE)
  add_subdirectory(mac)
endif()

# API documentation (docs/README.md).
sigil_add_docs(
  NAME SpellCircle
  BRIEF "The receiver product: scenes arriving over UDP, drawn with Skia, published over Syphon"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/shared/scene/include
        ${CMAKE_CURRENT_SOURCE_DIR}/qt/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/../../README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/shared/scene/include
        ${CMAKE_CURRENT_SOURCE_DIR}/qt/include)
