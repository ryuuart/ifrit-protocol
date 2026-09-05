# The draws of a finished layout — draw() one blob per run and pass,
# drawBatched() one drawGlyphs call per (font, paint) bucket and pass —
# with the decoration bands beneath and above the glyphs. The members are
# declared on ParagraphLayout in the layout feature's header and defined
# here, so a program that draws links this archive; paint/Paint.h is the
# feature's face: the same draws as free functions, and the resolver a
# pass with a material is shaded through, which this archive does not
# supply — it links no renderer.
sigil_library(SigilWeavePaint
  SOURCES Paint.cpp
  HEADERS Paint.h
  PUBLIC SigilWeaveDecoration)

# What reaches the canvas, one file per subject: the passes a layout draws
# with, and the decoration bands beneath and above the glyphs. Where a band
# lands is geometry and belongs to the decoration feature; only whether the
# ink arrives is asked here. A pass carrying a material shades through a
# resolver the host installs, so the kit paints and the Skia backend that
# shades them are both linked.
sigil_test(weave_paint_test
  SOURCES test/PaintTest.cpp test/DecorationPaintTest.cpp
  LIBRARIES SigilWeavePaint SigilWeavePorts SigilMaterialKit SigilMaterialSkia)

# draw and drawBatched per glyph on a raster surface.
sigil_bench(weave_paint_bench
  SOURCES bench/PaintBench.cpp
  LIBRARIES SigilWeavePaint SigilWeavePorts SigilMaterialKit SigilMaterialSkia)
