# The door a drawn frame leaves by: one abstract publisher over the
# graphics API's own texture and command-buffer handles, and one factory
# that answers with whatever this build can publish over.
#
# IT KNOWS NO WINDOW, NO TOOLKIT AND NO DRAWING. The handles cross the
# seam as opaque pointers, so a host that owns its device and a host
# drawing inside someone else's window reach the same publisher, and the
# header compiles wherever this repository compiles. Syphon is the macOS
# publication; elsewhere the factory has nothing to answer with and says
# so, which is an ordinary answer a caller reports rather than works
# around.
set(SIGIL_SKETCH_PUBLISH_SOURCES Publisher.cpp)
if(APPLE)
  find_package(unofficial-syphon CONFIG REQUIRED)
  list(APPEND SIGIL_SKETCH_PUBLISH_SOURCES SyphonPublisher.mm SyphonPublisher.h)
endif()

sigil_library(SigilSketchPublish
  SOURCES ${SIGIL_SKETCH_PUBLISH_SOURCES}
  HEADERS Publisher.h
  FRAMEWORKS Foundation Metal)

# The server owns the reference alloc/init handed it and releases it
# explicitly, so this target is deliberately not compiled with automatic
# reference counting.
if(APPLE)
  target_link_libraries(SigilSketchPublish PRIVATE unofficial::syphon::syphon)
endif()

# What can be asserted with no device: the factory's refusals. A server
# that really publishes wants a device and a subscriber, neither of which
# a test binary has.
sigil_test(sketch_test
  SOURCES test/PublisherTest.cpp
  LIBRARIES SigilSketchPublish)
