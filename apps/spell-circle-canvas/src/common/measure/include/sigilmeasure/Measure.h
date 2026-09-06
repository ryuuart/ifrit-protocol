#pragma once

/** @file
 * Every SigilMeasure header at once: the stopwatch, lap timer and frame
 * timer, the sample ring, counters, the frame sample, the running
 * moments, the histogram, the derived rescaling, the least-squares line
 * fit, and checks.
 */

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
