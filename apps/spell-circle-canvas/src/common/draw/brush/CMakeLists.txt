# SigilDrawBrush — procedural tools over the pen's public verbs, one
# subject per file: the tool and its pressure envelope, the dab sampler,
# the pressure-bearing stroke, the direction-field seam and its stock
# fields, the deposition executors, the hatch, wash and mass interiors, the
# stored polygon, plot and cursor, and the engine that holds a selection
# of all of them. Reaches down to the pen, and to SigilSkia's direct
# drawing for the sprite batch a round tip goes down as; a caller with its
# own tools links SigilDraw alone.
sigil_library(SigilDrawBrush PIC
  SOURCES Catalogue.cpp Choice.cpp Deposit.cpp Engine.cpp EngineStrokes.cpp
          EngineSurfaces.cpp Fibres.cpp Fields.cpp Hatch.cpp Mass.cpp
          Plot.cpp Polygon.cpp Position.cpp Pressure.cpp Sampler.cpp
          Stamps.cpp Stroke.cpp Tips.cpp Tool.cpp Wash.cpp
          DabStyle.h Executors.h HatchLines.h PenUnits.h PolygonMath.h
  HEADERS Brush.h Catalogue.h Choice.h Dab.h Deposit.h Engine.h Field.h
          Fields.h Hatch.h Mass.h Plot.h Polygon.h Position.h Pressure.h
          Sampler.h Stroke.h Tool.h Wash.h
  PUBLIC SigilDraw Boost::unordered
  PRIVATE SigilSkiaDraw)

sigil_test(draw_brush_test
  SOURCES test/CatalogueTest.cpp test/DepositTest.cpp test/EngineTest.cpp
          test/FieldsTest.cpp test/HatchTest.cpp test/MassTest.cpp
          test/PlotTest.cpp test/PolygonTest.cpp test/PositionTest.cpp
          test/SamplerTest.cpp test/StrokeTest.cpp test/ToolTest.cpp
          test/WashTest.cpp
  LIBRARIES SigilDrawBrush SigilWeavePorts)

sigil_bench(draw_brush_bench
  SOURCES bench/BrushBench.cpp
  LIBRARIES SigilDrawBrush)
