# The draw runtime: the immediate-mode sketch surface and the session
# that drives it — a clock, a pen and a surface that keeps what earlier
# frames drew, with one sketch body drawing into it every frame.
sigil_test(sketch_test
  SOURCES test/DrawSessionTest.cpp
  LIBRARIES SigilSketch SigilWeavePorts)

sigil_bench(sketch_bench
  SOURCES bench/DrawSessionBench.cpp
  LIBRARIES SigilSketch SigilWeavePorts)
