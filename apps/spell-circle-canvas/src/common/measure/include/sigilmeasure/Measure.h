#pragma once

/** @file
 * SigilMeasure's front door: every tier-1 instrument in one include —
 * the stopwatch and a timed block; the summary, quantile and histogram of
 * a run in hand; the window, smoothed reading and rate of a live stream;
 * and the check table. Control a caller rarely needs — the frame timer's
 * lanes, running moments, rescaling, the line fit, lap timers, counters
 * and the check formatter — is under `sigilmeasure/advanced/`.
 */

#include <sigilmeasure/check/Check.h>
#include <sigilmeasure/stats/Histogram.h>
#include <sigilmeasure/stats/Quantile.h>
#include <sigilmeasure/stats/Rate.h>
#include <sigilmeasure/stats/Smoothed.h>
#include <sigilmeasure/stats/Summary.h>
#include <sigilmeasure/stats/Window.h>
#include <sigilmeasure/time/Stopwatch.h>
