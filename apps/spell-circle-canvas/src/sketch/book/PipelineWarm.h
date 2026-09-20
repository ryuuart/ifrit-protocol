#pragma once

/** @file
 * The device programs a sketch's first frame would otherwise wait for,
 * stood up before it: which runtime effects a recorded key may name,
 * the keys this machine wrote down last time, and the stock effect
 * stages built ahead of any draw.
 */

#include <filesystem>
#include <memory>
#include <string>

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
 *  sets are read from and written back to. */
void openPipelineWarmup(const std::filesystem::path& storeDirectory);

/** THE PROGRAMS, BUILT, on whichever thread calls this — which must not
 *  be the thread that presents, since standing a program up costs
 *  exactly what waiting for it would.
 *
 *  @p precompile is the helper the context made, and it is ALL this
 *  touches: the context itself belongs to the thread that draws, so a
 *  warm-up reaching for it from here would be two threads inside one
 *  context. Make the helper where the context is, hand it over, and let
 *  go of it. @p backend is that context's backend by name, read the
 *  same way and for the same reason — it is part of which recorded set
 *  is this machine's.
 *
 *  Replays the set recorded against the same declaration and the same
 *  backend, which is the whole of what the last run's draws needed.
 *  Where there is no such set — a fresh machine, an edited shader, a
 *  new Skia — it stands up the effect stages instead, so a first frame
 *  wearing one of them finds it standing.
 *
 *  Does nothing when `openPipelineWarmup` was not called. */
void warmStockPipelines(
    std::unique_ptr<skgpu::graphite::PrecompileContext> precompile,
    std::string backend);

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
 *  What the set is worth to that open follows what the sweep drew. A
 *  selection of a few sketches leaves those sketches' programs; the
 *  whole registry wants more than the ceiling a written set is cut at,
 *  and what survives the cut is what the run drew FIRST rather than
 *  anything chosen for the sketch someone opens next.
 *
 *  It never REPLACES a set. A run that opened the whole registry knows
 *  less about what the next launch will draw than a run that opened one
 *  sketch, so a store that already answers for this declaration keeps
 *  its answer and this run's set is dropped at the end.
 *
 *  Called after `openPipelineWarmup`, on the thread that owns @p
 *  context, once that context exists. */
void recordPipelinesForAColdStore(const skgpu::graphite::Context& context);

/** WRITES BACK WHAT THIS RUN'S DRAWS WANTED, and says on stderr what
 *  the run cost in programs. Called once, after the last context is
 *  gone, so the set is everything the run needed rather than everything
 *  it had needed by then.
 *
 *  ONLY the programs a draw asked for. A program stood up from the file
 *  is reported as built again, so writing back everything recorded
 *  would write back the replay as well and the set would become every
 *  program every sketch ever opened here needed — each one a program a
 *  later launch stands up before its canvas draws whether it is opening
 *  that sketch or not.
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
