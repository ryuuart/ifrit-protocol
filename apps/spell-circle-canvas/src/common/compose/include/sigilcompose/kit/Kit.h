#pragma once

/** @file
 * SigilCompose KIT — the umbrella include.
 *
 * The kit is a layer ON TOP of SigilCompose that changes none of it: no
 * new `ElementNode` state, no new reconciler equality, no new paint path.
 * Every component here is a free function or a designated-init aggregate
 * over the shipped API, which is what makes the bar different from
 * `EXTRACT.md`'s. A wrong entry in the core API is permanent — the corpus
 * has a monument to that in `layouts::stickerScatter`, which has zero
 * users, was refused in writing by the one scene that wanted it, and
 * concedes why in its own doc comment. A wrong kit component is simply not
 * included by the next study and costs nothing.
 *
 * The bar that IS enforced here: **three or more real hand-rolls in the
 * corpus, cited file:line, before anything is built.** Every header names
 * its evidence in its first thirty lines. Two candidates failed it and are
 * not here; see the extraction report.
 *
 * | header | component | evidence |
 * |---|---|---|
 * | `kit/Frame.h` | `Frame` — figure-local polar coordinates | 5 hand-rolls, 2 conventions |
 * | `kit/Frame.h` | `Grid` — the unit map (scale, origin, snap) | 4 hand-rolls |
 * | `kit/Divisions.h` | `ticks()` — a division ladder as one path | 6 hand-rolls |
 * | `kit/Divisions.h` | `chords()` — polygon sides as N contours | 3 uses, 2 files |
 * | `kit/PixelType.h` | the aliased bitmap-font bake | 3 hand-rolls, ~120 lines each |
 * | `kit/Legibility.h` | halo / shade / scrim | 7 sites, 6 files |
 *
 * Already shipped elsewhere and deliberately NOT duplicated here:
 * `console::panel` + `console::monoStyle` (`Console.h`) and
 * `debug::check` / `report` / `failures` (`Debug.h`) are the verification
 * plate; `studio::hex` / `type` / `pickFace` / `ramp` / `fmt`
 * (`Studio.h`) are the prelude; `util::stroke` / `disc` / `centred`
 * (`Util.h`) are the value spellings.
 */

#include "sigilcompose/kit/Divisions.h"
#include "sigilcompose/kit/Frame.h"
#include "sigilcompose/kit/Legibility.h"
#include "sigilcompose/kit/PixelType.h"
