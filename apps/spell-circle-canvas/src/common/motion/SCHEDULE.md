# SigilMotion — the schedules

The chapter on how a run of units shares one progress: the spread that
says it, the orderings it deals in, and the cascade that resolves it
against the counts a frame actually has. `README.md` beside this file is
the library. Nothing here reads a clock, which is the point.

## How N units share one progress

A `Spread` says how a run of units divides one master progress between
them — the delay between one and the next, the order they are dealt in,
how long one unit's own motion lasts, and whether the whole thing loops.
It says nothing about WHAT a unit is. `Cascade` resolves it against the
counts a frame actually has, and then answers per index:

```cpp
Spread spec{.eachMs = 60, .durationMs = 420};
spec.from = Spread::From::Center;

Cascade cascade;                     // reused in place across frames
cascade.build(spec, unitCount, 0);
for (uint32_t i = 0; i < unitCount; ++i)
  paint(i, cascade.localTime(master, i, 0));   // this unit's own 0→1
```

`master` is a float in [0, 1] the caller owns — a track's progress, a
lane, a bare `phase()`. That is the whole interface, and it is why the
schedule feature links no clock: nothing in it reads time, so a text
engine, a set mounting its children, a feed's rows and a study's loop
counter can all drive the same body from four different clocks.

`spanMs()` is the DECLARE-TIME half: what a progress transition's
duration has to be for the last beat to close exactly as the master
arrives at 1, before any of the units exist. `Cascade::totalMs` is the
same number off a resolved cascade, and the two agree because one body
computes both.

Four things a spread can be, in the order they override each other: an
even ladder (`eachMs`), a fixed total divided across whatever the count
turns out to be (`amountMs`), an irregular table of start times cut
against a recording (`cueMs`, which replaces the ladder, the order and
the distribution outright, and which `Spread::cues()` sets on a spread
already in hand), and a second spread nested inside every beat
of the first (`then()`, exactly one level deep). `rankBy` is the ORDER
said the same way: one number per unit — a radius, a role, a depth from a
root — and the ladder is dealt smallest first, ties opening together. It
replaces `from` and `seed` and yields to a cue table, and everything else
the spread says still applies. `loopMs` turns any of
them into a wrapping beat: each unit re-opens on its own cycle, phase-
offset by its start, and one sweep of the master 0→1 is one cycle.

`Cascade::beat()` is the schedule read BACK rather than driven — start
time, local time and whether the beat is running — for anything that has
to travel with a cascade without being one of its units: a playhead, a
travelling underline, a per-unit meter. Without it each of those restates
`i · eachMs` and stops agreeing with the engine the moment the cascade
nests or takes a table.
