#pragma once

/** @file
 * SigilCompose KIT — the umbrella include.
 *
 * The kit is a layer ON TOP of SigilCompose that changes none of it: no
 * new `ElementNode` state, no new reconciler equality, no new paint path.
 * Every component is a free function or a designated-init aggregate over
 * the public API.
 *
 * That boundary is enforced by the build, not by convention. The kit is
 * its own CMake library, `SigilComposeKit`, whose only include path is
 * SigilCompose's public headers — so a kit component cannot reach a
 * library internal even by accident, and the public headers are proven
 * sufficient by the fact that the kit compiles against nothing else.
 *
 * The two tiers also differ in what a mistake costs. A wrong entry in the
 * library's own API is effectively permanent, because callers depend on
 * its spelling; a wrong kit component is simply not `#include`d by the
 * next caller. So the bar here is lower on taste and specific on evidence:
 * a component is built when the same few lines have been written by hand
 * several times over, not when it would be nice to have.
 *
 * | header | component |
 * |---|---|
 * | `kit/Frame.h` | `Frame` — figure-local polar coordinates |
 * | `kit/Frame.h` | `Grid` — the unit map (scale, origin, snap) |
 * | `kit/Divisions.h` | `ticks()` — a division ladder as one path |
 * | `kit/Divisions.h` | `chords()` — polygon sides as N contours |
 * | `kit/PixelType.h` | the aliased bitmap-font bake |
 * | `kit/Legibility.h` | halo / shade / scrim |
 *
 * **`kit/Strokes.h` IS NOT INCLUDED HERE, AND THIS UMBRELLA DOES NOT GIVE
 * YOU IT.** That header carries the stroke grammar — `kit::brush::shapers`,
 * `kit::profile`, `kit::strands`, `kit::spans`, `kit::shapes`, and the
 * `kit::brush::presets` — and including this file brings in none of those
 * names. Include `sigilcompose/kit/Strokes.h` directly when you want them.
 *
 * Already shipped elsewhere and deliberately NOT duplicated here:
 * `feed::plate` and `feed::tinted` (`Feed.h`) and
 * `debug::check` / `report` / `failures` (`Debug.h`) are the verification
 * plate; `studio::hex` / `type` / `pickFace` / `ramp` / `fmt`
 * (`Studio.h`) are the prelude; `util::stroke` / `disc` / `centred`
 * (`Util.h`) are the value spellings.
 */

#include "sigilcompose/kit/Divisions.h"
#include "sigilcompose/kit/Frame.h"
#include "sigilcompose/kit/Legibility.h"
#include "sigilcompose/kit/PixelType.h"
