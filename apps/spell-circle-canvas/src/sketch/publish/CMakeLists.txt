# BOTH ENDS OF ONE DOOR: one abstract publisher over the graphics API's
# own texture and command-buffer handles, one abstract subscription over
# the same kind of handle coming the other way, and one factory each that
# answers with whatever this build can carry a frame over.
#
# IT KNOWS NO WINDOW, NO TOOLKIT AND NO DRAWING. The handles cross the
# seam as opaque pointers, so a host that owns its device and a host
# drawing inside someone else's window reach the same publisher, and the
# headers compile wherever this repository compiles. Syphon is the macOS
# protocol at both ends; elsewhere the factories have nothing to answer
# with and say so, which is an ordinary answer a caller reports rather
# than works around.
set(SIGIL_SKETCH_PUBLISH_SOURCES Publisher.cpp Subscription.cpp)
if(APPLE)
  find_package(unofficial-syphon CONFIG REQUIRED)
  list(APPEND SIGIL_SKETCH_PUBLISH_SOURCES
       SyphonPublisher.mm SyphonPublisher.h
       SyphonSubscription.mm SyphonSubscription.h)
endif()

sigil_library(SigilSketchPublish
  SOURCES ${SIGIL_SKETCH_PUBLISH_SOURCES}
  HEADERS Publisher.h Subscription.h
  FRAMEWORKS Foundation Metal)

# The server owns the reference alloc/init handed it and releases it
# explicitly, and so does the client behind the subscription — which owns
# every frame it hands out until the next one is asked for — so this
# target is deliberately not compiled with automatic reference counting.
if(APPLE)
  target_link_libraries(SigilSketchPublish PRIVATE unofficial::syphon::syphon)
endif()

# What can be asserted with no device: the two factories' refusals. A
# server that really publishes wants a device and a subscriber, and a
# subscription that really opens wants a device and another application
# publishing — and a test binary has none of it.
sigil_test(sketch_test
  SOURCES test/PublisherTest.cpp test/SubscriptionTest.cpp
  LIBRARIES SigilSketchPublish)

# RECEIVER: THE OTHER SIDE OF THE SAME DOOR — the subscriber, so what a
# publisher offers can be looked at and written down without a third
# application in the middle. AppKit and Metal, no Qt and no sketch: it
# knows a name, a window and a file. It is an application rather than part
# of the archive above because nothing that publishes needs it, and it
# stands here rather than beside Sketchbook because the two ends of one
# protocol are one subject.
if(APPLE)
  sigil_frameworks(SIGIL_SKETCH_RECEIVER_FRAMEWORKS
                   Foundation AppKit Metal MetalKit QuartzCore)
  # Every one of these talks to Objective-C and every one of them is
  # handed references it owns — a frame image arrives at +1 — so they are
  # compiled with automatic reference counting. The command line is plain
  # C++ and is left out of it.
  set(SIGIL_SKETCH_RECEIVER_ARC_SOURCES
    receiver/Capture.mm receiver/Grab.mm receiver/main.mm
    receiver/Servers.mm receiver/Window.mm)
  add_executable(Receiver MACOSX_BUNDLE
    ${SIGIL_SKETCH_RECEIVER_ARC_SOURCES}
    receiver/Arguments.cpp
    receiver/Arguments.h receiver/Capture.h receiver/Grab.h
    receiver/Servers.h receiver/Window.h)
  set_source_files_properties(${SIGIL_SKETCH_RECEIVER_ARC_SOURCES}
    TARGET_DIRECTORY Receiver PROPERTIES COMPILE_OPTIONS -fobjc-arc)
  # A real .app: a bare Mach-O gets no bundle identity, so macOS gives the
  # window the executable's name and treats activation as a plain process.
  # Every verb still works through the binary inside it
  # (bin/<config>/Receiver.app/Contents/MacOS).
  set_target_properties(Receiver PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.sigil.Receiver"
    MACOSX_BUNDLE_BUNDLE_NAME "Receiver"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING
      ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE)
  target_include_directories(Receiver PRIVATE ${CMAKE_CURRENT_LIST_DIR})
  # THE SUBSCRIPTION IS THE ARCHIVE'S, not this application's: what it
  # means to hold a publication by its name is one subject, and the
  # window and the grab here are two readers of it. Beside it, the
  # encoder — a frame that has to become a file is pixels in and bytes
  # out, and where the bytes go is the byte sink's half. Syphon itself is
  # still named because the listing asks the directory directly.
  target_link_libraries(Receiver PRIVATE
    SigilSketchPublish SigilImageEncode SigilIOSource
    unofficial::syphon::syphon ${SIGIL_SKETCH_RECEIVER_FRAMEWORKS})

  # What the receiver can be asked with nothing publishing: the command
  # line, and the write, over a texture the case draws itself. The write
  # wants a device, so its suite carries the label a machine without one
  # leaves out.
  sigil_test(sketch_test
    ARC
    SOURCES test/ReceiverTest.mm receiver/Arguments.cpp receiver/Capture.mm
    LIBRARIES SigilImageEncode SigilImageDecode SigilIOSource
              ${SIGIL_SKETCH_RECEIVER_FRAMEWORKS}
    SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}
    SUITES ReceiverCapture LABELS gpu)
endif()
