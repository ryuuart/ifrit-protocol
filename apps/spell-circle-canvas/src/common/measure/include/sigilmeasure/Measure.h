#pragma once

/** @file
 * Every SigilMeasure header at once: the stopwatch, lap timer and frame
 * timer, the sample ring, counters, the frame sample, the running
 * moments, the histogram, the derived rescaling, the least-squares line
 * fit, and checks.
 */

/** @defgroup measure-time Timing
 *  The clocks a run is measured by: a stopwatch over one span, a lap
 *  timer over the phases of one, and the frame timer a render loop lays
 *  its marks in.
 *  @{ */
/** @} */

/** @defgroup measure-stats Statistics
 *  What a run of numbers amounts to: a rolling ring and its quantiles,
 *  running moments that keep no values, a histogram, named counters,
 *  the straight-line map that puts runs of different units on a common
 *  footing, and the least-squares line through a run.
 *  @{ */
/** @} */

/** @defgroup measure-check Check reporting
 *  A claim or a finding about a run, and the table they print as.
 *  @{ */
/** @} */

#include <sigilmeasure/check/Check.h>
#include <sigilmeasure/stats/Counters.h>
#include <sigilmeasure/stats/Fit.h>
#include <sigilmeasure/stats/FrameSample.h>
#include <sigilmeasure/stats/Histogram.h>
#include <sigilmeasure/stats/Moments.h>
#include <sigilmeasure/stats/Rescale.h>
#include <sigilmeasure/stats/Samples.h>
#include <sigilmeasure/time/FrameTimer.h>
#include <sigilmeasure/time/Laps.h>
#include <sigilmeasure/time/Stopwatch.h>
