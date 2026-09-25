# The time unit: `Duration`, the std::chrono duration every length, delay,
# step and period in the library is stated in. Header-only and over the
# standard library alone, so every feature — physics included, which must
# not link a clock — can state a time without linking anything that
# reads one.
sigil_library(SigilMotionTime
  HEADERS Duration.h)
