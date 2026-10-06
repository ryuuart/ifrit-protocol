#pragma once

/** @file
 * THE MACHINE'S CONDITIONS beside a measured rate: what powers it, and how
 * busy its GPU is. A rate taken on battery, with the GPU held in a low
 * performance state, is the machine's and not the sketch's, so a row that
 * measures says which conditions it was taken under.
 */

#include <string>

/** What powers the machine: "AC", "battery", or "unknown" where the
 *  platform does not say. */
std::string powerSource();

/** The GPU's utilization, in percent, as its driver reports it at this
 *  moment; negative where the platform does not report one. It is a
 *  reading of how busy the GPU was, not of the performance state it is
 *  clocked at, which no public interface reports. */
int gpuBusyPercent();
