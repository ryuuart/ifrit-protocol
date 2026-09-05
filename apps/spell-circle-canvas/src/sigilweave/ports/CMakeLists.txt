# Platform glue kept out of the engine: the system-font-manager factory
# (CoreText on Apple; DirectWrite and Fontconfig slot into SystemFontManager.cpp).
# Every engine feature accepts any SkFontMgr and never links this; only
# tools and apps that want the OS fonts do.
find_package(boost_unordered CONFIG REQUIRED)
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilWeavePorts
  SOURCES SystemFontManager.cpp
  HEADERS SystemFontManager.h
  PUBLIC unofficial::skia::skia
  PRIVATE Boost::unordered)

# The one binary in the tree whose subject IS the machine's font list, so
# it is the one place a claim about what the system answers belongs, and
# the label says what a runner must supply. It links this target alone:
# nothing here shapes, measures or lays anything out. The committed
# instrument faces are read through the same manager, which is what makes
# the file-loading claim answerable on a machine with no fonts at all.
sigil_test(weave_test
  SOURCES test/SystemFontManagerTest.cpp
  LIBRARIES SigilWeavePorts
  LABELS fonts)
