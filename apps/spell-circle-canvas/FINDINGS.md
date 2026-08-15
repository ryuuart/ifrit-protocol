# Findings

A work queue: each entry states what the code does, what it was evidently
intended to do, and what a test should assert once intent is restored.
Delete entries as they are fixed; delete the file when it is empty.

## `Ops.PathfinderBooleans` aborts under the address-sanitizer lane

**What the code does.** `scripts/sanitize.py --filter shape_test` builds
`shape_test` instrumented and the run dies in
`Ops_PathfinderBooleans_Test` with `SEGV in sk_realloc_throw` under
`SkOpBuilder::add` (`ops::unite`, `Ops.cpp`), inside Skia's prebuilt,
uninstrumented archive. The same test passes in the ordinary build, and
every other `shape_test` case passes under the sanitizer
(`--gtest_filter=-Ops.PathfinderBooleans`).

**What it was evidently intended to do.** The sanitizer lane exists to run
this repository's sources instrumented against uninstrumented vcpkg
archives; the top-level notes already pin Abseil's layout for that reason.
`SkOpBuilder` growing an `SkTDArray` across that boundary is the same
class of ABI/allocator mismatch, not a defect in `ops::unite` itself —
but the lane cannot currently vouch for the pathops code path.

**What a test should assert.** `Ops.PathfinderBooleans` runs green under
`scripts/sanitize.py` — either by pinning whatever Skia layout the
instrumented side disagrees on (as done for Abseil) or by routing the
booleans through an entry point that does not realloc across the
boundary. Until then the sanitizer lane's `shape_test` verdict excludes
this one case.
