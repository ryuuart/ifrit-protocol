#pragma once

/** @file
 * The device programs a sketch's first frame would otherwise wait for,
 * stood up before it: which runtime effects a recorded key may name, the
 * keys each sketch's draws asked for last time, and the stock effect
 * stages built ahead of any draw on a machine with nothing written down.
 */

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace skgpu::graphite {
class Context;
class PrecompileContext;
}  // namespace skgpu::graphite

/** DECLARES THE RUNTIME EFFECTS AND OPENS THE RECORDING.
 *
 *  Called once, on the thread the process starts on, AFTER the material
 *  warm-up has joined — the effects have to be compiled before they can
 *  be named — and BEFORE the first Graphite context is created, because
 *  a context is given the declaration and the recorder as it is built
 *  and neither reaches one that already exists.
 *
 *  What it declares is every SkSL body the effects are made of and
 *  every stock recipe's program: a key that names an undeclared effect
 *  cannot be written down at all, so anything left out of this list is
 *  left out of every recorded set. @p storeDirectory is where those
 *  sets are read from and written back to; @p namesEveryProgram puts
 *  one line on stderr per program built or replayed, naming it. */
void openPipelineWarmup(const std::filesystem::path& storeDirectory,
                        bool namesEveryProgram = false);

/** THE STOCK PROGRAMS, BUILT, on whichever thread calls this — which
 *  must not be the thread that presents, since standing a program up
 *  costs exactly what waiting for it would.
 *
 *  @p precompile is the helper the context made, and it is ALL this
 *  touches: the context itself belongs to the thread that draws, so a
 *  warm-up reaching for it from here would be two threads inside one
 *  context. Make the helper where the context is, hand it over, and let
 *  go of it. @p backend is that context's backend by name, read the
 *  same way and for the same reason — it is part of which recorded set
 *  is this machine's.
 *
 *  A store that holds a sketch's set for the same declaration and the
 *  same backend is answered sketch by sketch, by `warmSketchPipelines`
 *  before each opens, and this builds nothing. Where there is none — a
 *  fresh machine, an edited shader, a new Skia — it stands up the effect
 *  stages, so a first frame wearing one of them finds it standing.
 *
 *  Does nothing when `openPipelineWarmup` was not called. */
void warmStockPipelines(
    std::unique_ptr<skgpu::graphite::PrecompileContext> precompile,
    std::string backend);

/** WHETHER A SET IS WRITTEN DOWN FOR @p sketch — the sketch as the host
 *  names it before opening it, a registry key or a file's path —
 *  against this declaration and @p backend. False when
 *  `openPipelineWarmup` was not called. */
bool hasRecordedPipelines(std::string_view sketch, const std::string& backend);

/** THE PROGRAMS @p sketch's DRAWS ASKED FOR LAST TIME, BUILT, before the
 *  sketch opens — on whichever thread calls this, which must not be the
 *  thread that presents, through @p precompile, the helper the context
 *  made, exactly as `warmStockPipelines` is handed one.
 *
 *  The host's name for the sketch is all it needs: the set holds every
 *  program the sketch's draws wanted, its stacks inlined into theirs, so
 *  nothing about the sketch has to be read first, and a first frame drawn
 *  after it builds none of them. A key that no longer describes what it
 *  described is dropped rather than built as something else. */
void warmSketchPipelines(
    std::unique_ptr<skgpu::graphite::PrecompileContext> precompile,
    std::string backend, std::string sketch);

/** THE SKETCH WHOSE DRAWS COME NEXT, named as the host names it: every
 *  program a draw asks for from here on is written down under it, and
 *  its first frame's are counted until `pipelinesFirstFrameDrawn`. */
void pipelinesDrawnFor(std::string sketch);

/** THE FIRST FRAME OF THE SKETCH NAMED LAST HAS BEEN SUBMITTED: one line
 *  on stderr says how many programs its draws had to build. Once per
 *  `pipelinesDrawnFor`. */
void pipelinesFirstFrameDrawn();

/** LETS A LANE WITH NO FRAME TO HOLD WRITE ITS PROGRAMS DOWN, for a
 *  machine that has none written down yet.
 *
 *  The headless sweep on the device is that lane: it draws the programs
 *  an interactive run draws and draws them where nobody is waiting, so
 *  the set it leaves is what a first interactive open finds instead of
 *  nothing. It stands nothing up ahead of itself — there is no frame to
 *  protect, and replaying would put the store's state inside a lane
 *  whose output has to depend on nothing but the sketch.
 *
 *  Each sketch it draws leaves its own set, named by
 *  `pipelinesDrawnFor`, so a sweep of the whole registry seeds every
 *  sketch it drew.
 *
 *  It never REPLACES a set. A capture draws the sketch at its moment,
 *  which is less than a window that showed it draws, so a sketch that
 *  already has a set keeps it and this run's set for it is dropped at
 *  the end.
 *
 *  Called after `openPipelineWarmup`, on the thread that owns @p
 *  context, once that context exists. */
void recordPipelinesForAColdStore(const skgpu::graphite::Context& context);

/** WRITES BACK WHAT EACH SKETCH'S DRAWS WANTED, one set per sketch, and
 *  says on stderr what the run cost in programs. Called once, after the
 *  last context is gone, so each set is everything the sketch needed
 *  rather than everything it had needed by then.
 *
 *  ONLY the programs a draw asked for. A program stood up from the file
 *  is reported as built again, so writing back everything recorded
 *  would write back the replay as well and a sketch's set would become
 *  every program every sketch ever opened here needed.
 *
 *  Says nothing where no context was ever warmed — a window with no
 *  Graphite behind it neither records nor replays, and a tally of
 *  zeroes on such a lane says only that. */
void finishPipelineWarmup();

/** WHAT A BACKEND IS CALLED for the name a recorded set answers to: a
 *  key describes a program for one backend and means nothing to
 *  another. Read on the thread that owns the context, and handed to
 *  `warmStockPipelines` with the helper. */
std::string graphiteBackendName(const skgpu::graphite::Context& context);
