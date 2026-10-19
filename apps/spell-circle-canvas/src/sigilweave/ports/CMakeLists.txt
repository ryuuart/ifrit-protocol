# Platform glue kept out of the engine: the system-font-manager factory
# (CoreText on Apple; DirectWrite and Fontconfig slot into SystemFontManager.cpp).
# Every engine feature accepts any SkFontMgr and never links this; only
# tools and apps that want the OS fonts do. It answers faces, so the style
# feature that defines a face is public to it, and its catalogue reads the
# faces a font context adopted, so the font service is public to it too.
find_package(boost_unordered CONFIG REQUIRED)
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilWeavePorts
  SOURCES SystemFontManager.cpp GenericFamilyManager.cpp
          GenericFamilyManager.h Resource.cpp Catalogue.cpp
  HEADERS SystemFontManager.h Resource.h Catalogue.h
  PUBLIC SigilWeaveStyle SigilWeaveFonts unofficial::skia::skia
  PRIVATE Boost::unordered)

# The one binary in the tree whose subject IS the machine's font list, so
# it is the one place a claim about what the system answers belongs, and
# the label on the cases that walk the machine's families says what a
# runner must supply. It links this target alone: nothing here shapes,
# measures or lays anything out. The committed instrument faces are read
# through the same manager, which is what makes the file-loading claim
# answerable on a machine with no fonts at all, so it carries no label.
sigil_test(weave_test
  SOURCES test/SystemFontManagerTest.cpp
  LIBRARIES SigilWeavePorts
  LABELS fonts
  SUITES InstalledFonts Asks/HeldFace)

# The platform's other half: the C++ runtime the whole binary's guards
# are written against, which a linked archive can disable for every one
# of them at once. It needs no font and no machine, so it carries no
# label.
sigil_test(weave_test
  SOURCES test/StandardExceptionsTest.cpp
  LIBRARIES SigilWeavePorts)

# A font file's bytes as a face, read from the committed instrument faces,
# so it needs no installed font either.
sigil_test(weave_test
  SOURCES test/ResourceTest.cpp
  LIBRARIES SigilWeavePorts)

# The catalogue: the machine's own families read as styles and axes, which
# is what the `fonts` label says, and a context's adopted faces among them,
# read from the committed instrument faces.
sigil_test(weave_test
  SOURCES test/CatalogueTest.cpp
  LIBRARIES SigilWeavePorts
  LABELS fonts
  SUITES TypeCatalogue)
