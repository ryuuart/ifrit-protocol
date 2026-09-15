# The wires themselves: the feeds a session has open and what each of
# them is doing, the log of one wire's messages, the way out through a
# peer, the recording a wire is written down to, and the seven readings
# a message is shown through.
#
# PUBLIC is what the headers spell: SigilIO's feeds and its byte
# vocabulary, and SigilData's decoders, because the schema a wire is read
# through is one of their values and whoever hands one over has to be
# able to make one. The transports are PRIVATE because no header of this
# library names one — a scheme is a string here. What a reading ANSWERS
# is a string and what a spelled message answers is bytes, so linking
# this makes nobody speak in document types.
sigil_library(SigilSeerWire
  SOURCES Log.cpp Recorder.cpp Rendering.cpp Sender.cpp Wires.cpp
  HEADERS Log.h Recorder.h Rendering.h Sender.h Wires.h
  PUBLIC SigilIO SigilDataDecode
  PRIVATE SigilIOTransport)

# The schema reading's cases read a schema of their own, generated the
# way a sketch's is: the header for building a message of it, and the
# binary schema beside it as the FILE a reader hands the tool, which is
# the whole of what the reading promises — a schema no generated header
# in this binary was compiled from.
find_package(flatbuffers CONFIG REQUIRED)
set(SEER_TEST_SCHEMA ${CMAKE_CURRENT_LIST_DIR}/test/seer_test.fbs)
set(SEER_TEST_SCHEMA_OUT ${CMAKE_CURRENT_BINARY_DIR}/schema)
add_custom_command(
  OUTPUT ${SEER_TEST_SCHEMA_OUT}/seer_test_generated.h
         ${SEER_TEST_SCHEMA_OUT}/seer_test_bfbs_generated.h
         ${SEER_TEST_SCHEMA_OUT}/seer_test.bfbs
  COMMAND ${CMAKE_COMMAND} -E make_directory ${SEER_TEST_SCHEMA_OUT}
  COMMAND flatbuffers::flatc --cpp -b --schema --bfbs-gen-embed
          -o ${SEER_TEST_SCHEMA_OUT} ${SEER_TEST_SCHEMA}
  DEPENDS ${SEER_TEST_SCHEMA}
  COMMENT "Generating seer_test_generated.h from the test schema"
  VERBATIM)
# The rule enters the build graph through a target, which the binary the
# cases are compiled into waits on.
add_custom_target(SigilSeerTestSchema
  DEPENDS ${SEER_TEST_SCHEMA_OUT}/seer_test_generated.h)

# What a wire promises: a URI nobody can open is still a row that says
# why, two wires on the loopback carry bytes between them in order, a
# message is kept with the sender it was delivered with, every wire
# names the sender of its newest message without anyone draining it, a
# tick reads a rate off a wire without a clock of its own, a recording
# is a file that reads back and a wire that plays it as time moves
# forward, each reading of a message is empty exactly when the message
# is not that, a message a reader spells is the bytes its codec writes,
# and a wire says what it speaks off its URI alone. No toolkit: every one
# of them runs with no application.
#
# The codecs are named here and not in the library: what the dialect
# cases assert is that a reading and a spelling agree with the bytes the
# codec itself writes, which is a claim about both and cannot be made
# against one of them alone. The FlatBuffers runtime is named for the
# same reason: a case builds the buffer it then reads back through a
# schema.
sigil_test(seer_test
  SOURCES test/DialectTest.cpp test/OscTest.cpp test/SchemaTest.cpp
          test/WiresTest.cpp
  LIBRARIES SigilSeerWire SigilDataDecode flatbuffers::flatbuffers
  SUPPORT_DIRS ${SEER_TEST_SCHEMA_OUT}
  DEFINITIONS SEER_TEST_SCHEMA_FILE="${SEER_TEST_SCHEMA_OUT}/seer_test.bfbs")
add_dependencies(seer_test SigilSeerTestSchema)
