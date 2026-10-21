/** @file
 * The machine's conditions beside a measured rate, read through the
 * platform's own power and accelerator registries, and the power
 * manager's hold that keeps the display lit while it is taken.
 */

#include "Conditions.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#endif

std::string powerSource() {
#ifdef __APPLE__
  CFTypeRef info = IOPSCopyPowerSourcesInfo();
  if (!info) return "unknown";
  std::string out = "unknown";
  if (CFStringRef type = IOPSGetProvidingPowerSourceType(info)) {
    if (CFStringCompare(type, CFSTR(kIOPMACPowerKey), 0) == kCFCompareEqualTo)
      out = "AC";
    else if (CFStringCompare(type, CFSTR(kIOPMBatteryPowerKey), 0) ==
             kCFCompareEqualTo)
      out = "battery";
  }
  CFRelease(info);
  return out;
#else
  return "unknown";
#endif
}

int gpuBusyPercent() {
#ifdef __APPLE__
  // Every accelerator the registry lists publishes a dictionary of
  // statistics; the first that says how much of the device is in use is
  // the reading.
  io_iterator_t accelerators = 0;
  if (IOServiceGetMatchingServices(kIOMainPortDefault,
                                   IOServiceMatching("IOAccelerator"),
                                   &accelerators) != KERN_SUCCESS)
    return -1;
  int busy = -1;
  for (io_object_t accelerator = IOIteratorNext(accelerators);
       accelerator != 0 && busy < 0;
       accelerator = IOIteratorNext(accelerators)) {
    CFTypeRef statistics = IORegistryEntryCreateCFProperty(
        accelerator, CFSTR("PerformanceStatistics"), kCFAllocatorDefault, 0);
    if (statistics && CFGetTypeID(statistics) == CFDictionaryGetTypeID()) {
      const auto value = static_cast<CFNumberRef>(
          CFDictionaryGetValue(static_cast<CFDictionaryRef>(statistics),
                               CFSTR("Device Utilization %")));
      int percent = 0;
      if (value && CFGetTypeID(value) == CFNumberGetTypeID() &&
          CFNumberGetValue(value, kCFNumberIntType, &percent))
        busy = percent;
    }
    if (statistics) CFRelease(statistics);
    IOObjectRelease(accelerator);
  }
  IOObjectRelease(accelerators);
  return busy;
#else
  return -1;
#endif
}

double mainDisplayPixelRatio() {
#ifdef __APPLE__
  CGDisplayModeRef mode = CGDisplayCopyDisplayMode(CGMainDisplayID());
  if (!mode) return 0.0;
  const size_t points = CGDisplayModeGetWidth(mode);
  const size_t pixels = CGDisplayModeGetPixelWidth(mode);
  CGDisplayModeRelease(mode);
  return points > 0 ? (double)pixels / (double)points : 0.0;
#else
  return 0.0;
#endif
}

DisplayAwakeHold::DisplayAwakeHold(const char* reason) {
#ifdef __APPLE__
  CFStringRef name =
      CFStringCreateWithCString(kCFAllocatorDefault, reason, kCFStringEncodingUTF8);
  // A display already asleep is not woken by a hold against idle sleep,
  // so the hold begins as a user's touch would: the display comes on,
  // and the hold keeps it on from there.
  IOPMAssertionID activity = kIOPMNullAssertionID;
  if (name &&
      IOPMAssertionDeclareUserActivity(name, kIOPMUserActiveLocal,
                                       &activity) == kIOReturnSuccess)
    m_activity = activity;
  IOPMAssertionID assertion = kIOPMNullAssertionID;
  if (name &&
      IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep,
                                  kIOPMAssertionLevelOn, name,
                                  &assertion) == kIOReturnSuccess)
    m_assertion = assertion;
  if (name) CFRelease(name);
#else
  (void)reason;
#endif
}

DisplayAwakeHold::~DisplayAwakeHold() {
#ifdef __APPLE__
  if (m_assertion != 0) IOPMAssertionRelease(m_assertion);
  if (m_activity != 0) IOPMAssertionRelease(m_activity);
#endif
}
