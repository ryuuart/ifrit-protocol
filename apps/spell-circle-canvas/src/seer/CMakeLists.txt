# SigilSeer — the tool that watches every wire a sketch listens on and
# sends down them.
#
# A wire is one URI a message arrives on or leaves by. The library is
# plain C++ over SigilIO's feeds: it opens them, reads what each of them
# is doing, keeps what came down one of them, writes one down to a file
# and plays a file back, and makes a message readable three ways. It
# knows no toolkit and no window, so every one of those claims is
# asserted without an application existing.
#
# NOTHING OF THE DRAWING LIBRARIES IS HERE, and nothing of SigilSketch.
# What travels a wire is bytes; what those bytes mean is the concern of
# whoever is at the other end, and a tool for looking at the wire itself
# must be able to look at a wire whose format it has never heard of.
#
# Two targets: the archive below, and the application over it in app/.
sigil_library_root(SigilSeer
  BRIEF "Seer: every wire a sketch listens on, what arrives there, and what is sent back")

add_subdirectory(wire)

# Seer: the one window over the wires.
add_subdirectory(app)

# Every API name the README spells, compiled against the headers that own
# it. SigilIO's include root is scanned because the prose names the feed
# and the hub a wire stands on, and SigilData's because it names the
# schema a wire is read through; both are what this library's public
# link carries.
sigil_doc_probes(seer
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../common/io/include/sigilio
           ${CMAKE_CURRENT_SOURCE_DIR}/../common/data/include/sigildata
  PRELUDES "<sigilio/hub/Feed.h>" "<sigilio/hub/Hub.h>"
           "<sigildata/decode/FlatBuffer.h>"
  LIBRARIES SigilSeerWire SigilDataDecode)
