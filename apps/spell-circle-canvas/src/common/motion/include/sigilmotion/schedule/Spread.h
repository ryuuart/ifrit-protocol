#pragma once

/** @file
 * The SPEC of a cascade: how a run of N units share out one master
 * progress. Delays, the order they are dealt in, how long one unit's own
 * motion lasts, and whether the whole thing loops.
 *
 * A spread says nothing about WHAT a unit is. It numbers units 0…N−1 and
 * answers in milliseconds; the host decides whether index 7 is a glyph, a
 * feed row, a child node or a tile.
 */

#include <choreograph/Choreograph.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace sigil::motion {

/** HOW N UNITS SHARE OUT ONE PROGRESS.
 *
 *  Every field is milliseconds, an index or a curve. Hand it to a
 *  `Cascade` with the counts a frame actually has, and the cascade
 *  answers each unit's start time and each unit's own local 0→1. */
struct Spread {
  /** Between one unit's start and the next's. */
  float eachMs = 30;
  /** Amount-mode (mutually exclusive with eachMs; wins when > 0): the
   *  TOTAL spread, divided across however many units there are. Use it
   *  when the budget for the whole entrance is fixed and the count may
   *  change — `eachMs` keeps per-unit spacing and lets the total grow,
   *  this keeps the total and shrinks the spacing. */
  float amountMs = 0;
  /** AN IRREGULAR TABLE: one start time per unit, in ms from the start of
   *  the master progress, read by unit index. Caption, lyric and lip-sync
   *  timing is a table cut against a recording, and no even spread is a
   *  substitute for one.
   *
   *  Non-empty, it REPLACES the even spread: `eachMs`, `amountMs`, `from`
   *  and `distribution` say nothing, because a table already states both
   *  the order and the shape of the cascade. Everything else this struct
   *  says still holds — `durationMs` is still how long one unit's own
   *  motion lasts, and a nested spread still runs inside every beat.
   *
   *  A unit past the end of the table starts at the LAST entry, so a short
   *  table piles its tail on one beat rather than inventing times; entries
   *  past the last unit are ignored. Either mismatch warns once. */
  std::vector<float> cueMs;
  /** How long one unit's own motion lasts. */
  float durationMs = 450;
  /** THE PER-UNIT WRAPPING BEAT: set above 0 and the cascade LOOPS — every
   *  unit's beat RE-OPENS on its own cycle of this period, phase-offset by
   *  the unit's start time (even ladder and cue table alike), so steady
   *  continuous motion — rain re-dropping column by column, arrivals that
   *  never stop arriving — is DECLARED rather than faked by re-running a
   *  one-shot.
   *
   *  THE MASTER STILL CLOCKS IT, and one full sweep 0→1 is exactly ONE
   *  CYCLE: the master maps onto `loopMs` of virtual time instead of the
   *  one-shot span, and unit i reads
   *  `clamp(((master·loopMs − startMs_i) mod loopMs) / durationMs)`.
   *  Master 0 and master 1 name the same instant of the cycle, so a
   *  WRAPPING phase — an Output stepped mod 1 — drives it seamlessly
   *  forever.
   *
   *  BETWEEN a beat's close and its next opening the unit rests at local 1
   *  — its landed value — and returns to 0 the instant its beat re-opens,
   *  so an effect that loops cleanly ends where nothing shows. Start
   *  offsets FOLD mod the period: two units whose starts differ by exactly
   *  `loopMs` share a phase, and a period shorter than `durationMs`
   *  re-opens a beat before it lands. The fold also means every unit is
   *  ALWAYS somewhere in its cycle — there is no "before the first beat".
   *
   *  `spanMs` answers the PERIOD when this is set — still the ms the
   *  master maps onto, and the number a driver needs: wrap the phase every
   *  `loopMs` of wall time and the schedule runs at its authored ms. ONE
   *  loop governs the whole cascade, read off the OUTER spread; a nested
   *  loopMs is ignored. 0 — the default — is the one-shot cascade. */
  float loopMs = 0;
  /** Where the cascade starts. `Random` is keyed on the unit count and
   *  `seed`, so a scatter is the SAME scatter on every frame and after
   *  every rebuild; `Edges` starts at both ends and meets in the middle. */
  enum class From : uint8_t { Start, Center, End, Random, Edges } from =
      From::Start;
  /** WHICH scatter `From::Random` deals. The ranking hash is keyed on the
   *  unit count alone at the default 0, so two same-count cascades scatter
   *  IDENTICALLY — three curtains of equal columns would all drop in one
   *  order. A nonzero seed mixes into that hash and deals an independent
   *  scatter per value, which is what several fields of one composition
   *  want. The scatter stays the scrambled EVEN ladder either way: every
   *  unit takes a distinct rank, so no two units ever open together, and
   *  `distribution` still shapes how those ranks crowd. Read only under
   *  `From::Random`; the other origins are their own order. */
  uint32_t seed = 0;
  /** Shapes the START TIMES across the cascade (not the per-unit motion,
   *  which the driven value owns): the linear ramp of delays is passed
   *  through this curve, so an ease-in distribution crowds the early units
   *  together and lets the tail spread out. Null is the uniform spacing. */
  choreograph::EaseFn distribution = nullptr;
  /** A NESTED cascade inside each of this one's beats — see `then()`.
   *  Held out of line because a Spread cannot contain itself by value.
   *  Exactly ONE level deep: a cascade reads this and stops. */
  std::shared_ptr<const Spread> inner;

  /** Compounds a second cascade inside every beat of this one: delay each
   *  word, then delay each letter within its word's beat. The outer
   *  `durationMs` is ignored — a beat lasts exactly as long as the inner
   *  cascade needs — and so are the inner `loopMs` and any third level
   *  nested inside @p nested. */
  Spread& then(Spread nested);

  /** THE VIRTUAL SPAN, in ms: what a master progress [0,1] maps onto when
   *  this spread numbers @p count units — the moment the last beat closes.
   *  `durationMs + eachMs·(N−1)` for the even ladder, `durationMs +
   *  amountMs` past one unit in amount mode (every count past one answers
   *  the same, because the amount IS the spread), the latest time any unit
   *  reads out of a cue table plus `durationMs`, and the compounded extent
   *  under `then()`, where @p innerCount is how many inner units one beat
   *  holds (the widest beat's count, where they vary). Zero units answer as
   *  one unit does: `durationMs` alone.
   *
   *  A LOOPING spread (`loopMs` > 0) answers its PERIOD, whatever the
   *  counts.
   *
   *  This is the DECLARE-TIME form, for the number a description needs
   *  before any node exists — above all a progress transition whose
   *  duration should cover the cascade exactly, so the last beat closes as
   *  the master arrives at 1 and the schedule runs at its authored ms. A
   *  host that has already resolved a `Cascade` reads the same number off
   *  `Cascade::totalMs`; the two agree because one resolved-cascade body
   *  computes both. */
  [[nodiscard]] float spanMs(uint32_t count, uint32_t innerCount = 1) const;

  bool operator==(const Spread& other) const;
};

}  // namespace sigil::motion
