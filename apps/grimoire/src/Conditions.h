#pragma once

/** @file
 * THE MACHINE'S CONDITIONS beside a measured rate: what powers it, how
 * busy its GPU is, and whether the display stays lit while it is taken. A
 * rate taken on battery, with the GPU held in a low performance state, is
 * the machine's and not the sketch's, so a row that measures says which
 * conditions it was taken under.
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

/** How many pixels the main display packs into one of its points: 2 on a
 *  Retina panel, 1 on most external displays, 0 where the platform does
 *  not say. Read before the application exists, so a stated device pixel
 *  ratio can be asked of Qt as the factor that reaches it on the display
 *  a new window opens on. */
double mainDisplayPixelRatio();

/** THE DISPLAY LIT, and kept lit for as long as this is held: taking it
 *  wakes a display that has already gone to sleep.
 *
 *  A window on a display that has gone to sleep is no longer exposed, and
 *  the window system stops asking it for frames: a rate measured then is
 *  no frames at all, whatever the sketch costs. A display sleeps after the
 *  machine has seen no input for a while, which an unattended lane over
 *  the whole registry outlasts. Held by the window's frame-rate lane for
 *  its whole run; does nothing where the platform offers no such hold. */
class DisplayAwakeHold {
 public:
  explicit DisplayAwakeHold(const char* reason);
  ~DisplayAwakeHold();
  DisplayAwakeHold(const DisplayAwakeHold&) = delete;
  DisplayAwakeHold& operator=(const DisplayAwakeHold&) = delete;

 private:
  unsigned int m_assertion = 0;
  unsigned int m_activity = 0;
};
