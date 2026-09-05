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

# Every test here puts pixels on a surface, except the one that asks the
# preset runtime shaders to resolve. The material pass installs their
# resolver and shades with a kit paint, which is why both link here.
sigil_test(weave_paint_test
  SOURCES test/PaintTest.cpp test/DecorationPaintTest.cpp
          test/ShaderPresetTest.cpp
  LIBRARIES SigilWeavePaint SigilWeavePorts SigilMaterialKit SigilMaterialSkia)

# draw and drawBatched per glyph on a raster surface.
sigil_bench(weave_paint_bench
  SOURCES bench/PaintBench.cpp
  LIBRARIES SigilWeavePaint SigilWeavePorts SigilMaterialKit SigilMaterialSkia)
