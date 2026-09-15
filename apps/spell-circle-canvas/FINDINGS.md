# Findings

A work queue: each entry states what the code does, what it was evidently
intended to do, and what a test should assert once intent is restored.
Entries are deleted as they are fixed, and the file is deleted when it is
empty.

## live_settling's plate moves under a full sweep and holds alone

**What the code does.** In one whole-registry sweep, five jobs at once,
`live_settling` rendered with a different hash from its baseline; rendered
alone twice afterwards it was byte-identical to that baseline both times.
The sketch settles a web page through the settle machine, whose quiet
stage is a stretch of time with no repaint in it, and under the load of a
sweep the engine's repaints land later, so the frame the settle stops on
is not always the same frame.

**What it was intended to do.** A capture is a function of the
declaration alone; the settle's stages are judged on the page's own
events, and the quiet window was meant to be the one clock, long enough
that load does not move the frame it stops on.

**What a test should assert.** A settle on the same page, run under a
parallel load, stops on the same frame; the plate ledger's full sweep
and a single render of `live_settling` agree.

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
