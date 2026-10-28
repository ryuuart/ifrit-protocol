# The time unit: `Duration`, the std::chrono duration every length, delay,
# step and period in the library is stated in, and the CSS time reader.
# Both are SigilCore's compute leaf, named in this library's namespace, so
# every feature — physics included, which must not link a clock — can
# state a time without linking anything that reads one.
sigil_library(SigilMotionTime
  HEADERS Duration.h
  INTERFACE SigilCoreCompute)
