# Findings

A work queue: each entry states what the code does, what it was evidently
intended to do, and what a test should assert once intent is restored.
Entries are deleted as they are fixed, and the file is deleted when it is
empty.

## Sketch documentation probes include web headers when Ultralight is disabled

**What the code does.** Configuring with
`CMAKE_DISABLE_FIND_PACKAGE_Ultralight=ON` and building Sketchbook and
`sketch_test` fails while including `sigilsketch/scry/SettledPage.h`, whose
`sigilscry/engine/WebView.h` dependency is unavailable. The generated sketch
documentation probe includes the web headers even though the library omits
its web sources, public headers and tests when `SigilScry` is absent.

**What it was evidently intended to do.** The native host and its non-web
tests build without the optional web SDK. Documentation probes should check
the API surface present in that configuration. The Python extension target
can already build with the SDK disabled.

**What a test should assert.** Configure without Ultralight and build
Sketchbook, `sketch_test` and the sketch documentation probes successfully;
then configure with the SDK and verify that web API documentation is still
checked.

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

## A webrtc case that starts a peer process fails about once in twenty, at its own deadline to the millisecond

**What the code does.** The `IOWebRtc` cases that want a peer —
`APeersMessageArrivesOnTheOneWaitingNamingThePeerItCameFrom`,
`TheOneWaitingReachesThePeerWithOneSend`,
`SendToReachesTheOnePeerItNamesAndNoOther` and
`DroppingTheLastHolderOfAFeedGivesUpItsSignallingPort` — start this
binary again as a peer and then give the whole of it, the process start
and the handshake together, one deadline of ten seconds. Run as the
suite, in one process, on a machine with other work on it, one of them
fails about once in twenty runs and always at that deadline to the
millisecond: eighteen of twenty whole-suite runs passed with the ports
the doors now answer and seventeen of twenty with the ports the suite
picked for itself before, while those four cases run on their own
passed ten of ten either way. The peer process is running when it
happens — its own banner stands in the output and its own assertions
hold — so what has not happened is the pairing, which otherwise takes a
tenth of a second.

**What it was evidently intended to do.** The deadline covers a
handshake with room to spare, so a case fails when two ends cannot find
each other and never because the machine was busy.

**What a test should assert.** That the peer is UP is one wait and that
the two ends PAIRED is another: the peer says when it has opened its
feed, the case waits for that without a verdict attached, and only the
pairing after it is what the case passes or fails on. A case that
cannot pair once the peer is up is the transport failing and says so;
one whose peer never got going says that instead.
