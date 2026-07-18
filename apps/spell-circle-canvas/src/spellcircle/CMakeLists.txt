# SpellCircle — the scene receiver product. Two executables share one core:
#
#   shared/  Qt-free code both frontends embed: the wire schema, the ASIO
#            UDP transport, and the scene core (decode/resolve/draw).
#   qt/      The cross-platform Qt Quick receiver (executable SpellCircle).
#   mac/     The native SwiftUI receiver (executable SpellCircleMac,
#            macOS + Swift toolchain only).
add_subdirectory(shared/schema)
add_subdirectory(shared/net)
add_subdirectory(shared/scene)
add_subdirectory(qt)
if(APPLE)
  add_subdirectory(mac)
endif()
