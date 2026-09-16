# Findings

A work queue: each entry states what the code does, what it was evidently
intended to do, and what a test should assert once intent is restored.
Entries are deleted as they are fixed, and the file is deleted when it is
empty.

## live_settling's plate disagrees with itself: a line-break budget spent against a clock decides what is drawn

**What the code does.** `live_settling` renders to one of two plates
whether it is rendered alone or under a five-job sweep, and the ledger's
own stability pass — `plates --sketch live_settling --stability 4` —
reports it as disagreeing with ITSELF across five renders. One cell
differs, the fourth, the study declared `live(true, 1)`: it reads
`reused 0 degraded 1` in one plate and `reused 1 degraded 0` in the
other, and its passage is filled greedily in the first and set
optimally in the second. The floor under a live block is spent against
`std::chrono::steady_clock` in `knuthPlassBlock`
(`src/sigilweave/layout/KnuthPlass.cpp`), read once every eighth of the
block's words, so whether the first few break candidates take more or
less than the declared microsecond is a race with whatever else the
machine is doing. A block that finishes inside the floor puts its break
decisions in the per-thread store, and every later step of the swell at
that measure is answered from the store rather than composed — so one
won race changes the whole report and the whole setting, and the
sequence is not driven by a web page at all.

**What it was intended to do.** A capture is a function of the
declaration alone. The runtime's other stopwatch decision — the
composer's automatic texture promotion, which re-bakes by a measured
per-frame cost — is held off in a deterministic session for exactly
this reason, and `SketchContext::measured` pins the numbers a sketch
measured about its own execution; the line-break floor is the same
shape as the first and is held off by nothing. Either it joins the
promotion pin, which costs the fourth study its degrade and makes the
cell's note untrue of the plate, or the floor stops being a stretch of
clock and becomes a count of the work the breaker does — break
candidates examined — which is the same number on a loaded machine as
on an idle one and leaves every one of the four studies saying what it
says today. The second is a change to `Element::live`'s and
`KnuthPlassOptions`'s spelling, which is a naming call.

**What a test should assert.** Five renders of `live_settling` running
hash the same five times, which is what the ledger's stability pass
asks. At the unit level,
`ComposeSettling.ABudgetNothingCanMeetDegradesAndSaysSo` in
`src/common/compose/typography/test/ComposeTestParagraphs.cpp` asserts a
degrade out of a one-microsecond floor and is the same race in a test:
it should assert the degrade from a floor the breaker cannot meet by
count, and a second case should assert that the same swell run twice
reports the same settling.

## A base-class catch of a standard exception matches nothing in a binary that links Yoga or Skia

**What the code does.** `catch (const std::exception&)` lets a
`std::runtime_error` pass — and with it every exception whose typeinfo
lives in libc++, which is everything the standard library itself throws
— and the process ends, in every binary of this tree that links
`libyogacore.a` or `libskia.a`, which is every one that draws or shapes
text. Both archives were compiled without run-time type information, and
a throw site compiled that way emits its own hidden, non-unique copy of
the typeinfo of the standard types it throws: ten of Yoga's objects and
Skia's `raw.SkRawCodec.o` carry `typeinfo for std::exception`,
`std::logic_error`, `std::out_of_range`, `std::length_error`,
`std::bad_alloc` and `std::bad_array_new_length`. The linker satisfies
every other object's reference to those names with the hidden copy, and
the ARM64 runtime holds a non-unique typeinfo unequal to the unique one a
throw from libc++ carries, so the handler search finds no match. A catch
of the exact thrown type still matches, and a catch of an exception type
defined in a header matches, which is why nothing has shown it so far.
An eight-line program that throws `std::runtime_error` and catches
`const std::exception&` runs when linked alone and terminates when
linked with `-force_load libyogacore.a`.

**What it was intended to do.** A base-class catch is the guard this
tree writes around a parse, a conversion, a filesystem call or a library
that throws; it was meant to catch what the standard library throws.

**What a test should assert.** In `weave_test` and in `sketch_test`,
binaries that link Yoga and Skia, a case throws `std::runtime_error`,
catches `const std::exception&`, and passes. The fix is in the ports, in
the sigil-vcpkg-registry: Yoga compiled with run-time type information,
which its own build turns off, and Skia's raw codec likewise or left out
of the build, with the baseline bumped. Until then the WebRTC door's
guards are catch-alls, and every other base-class catch of a standard
exception in the tree is inert.
