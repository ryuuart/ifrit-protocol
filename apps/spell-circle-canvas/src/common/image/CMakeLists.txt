# SigilImage — Qt-free image import for the scene canvases, as one
# feature library per directory. Each feature is its own static archive
# with its own tests and benchmarks, and links only the features beneath
# it, so a consumer that needs one tier links that tier alone.
# `SigilImage` is the umbrella: an interface over every feature, for a
# consumer of the whole library.
#
# The committed fixtures every test and bench reads sit in test/assets/
# (one still per format plus a 3-frame red/green/blue animation for each
# animated format, regenerable with PIL + avifenc) and reach a binary
# through the SIGIL_TEST_ASSET_DIR compile definition; the helpers every
# image test reads a fixture by are test/Pixels.h.
#
# The Skia codec set is guaranteed by the skia vcpkg features declared in
# vcpkg.json (png/jpeg/webp are port defaults; avif is opted in there).
# The port's avif feature carries libavif[dav1d], because a libavif built
# with no AV1 codec parses an AVIF container and decodes no frame out of
# it.
sigil_library_root(SigilImage
  BRIEF "Image decoding and encoding: bytes in, Skia images out, and back")

# Dependencies point down this list.
add_subdirectory(asset)
add_subdirectory(decode)
add_subdirectory(encode)

sigil_library(SigilImage
  INTERFACE SigilImageAsset SigilImageDecode SigilImageEncode)
