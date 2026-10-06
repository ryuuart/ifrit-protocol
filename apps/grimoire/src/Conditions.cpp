/** @file
 * The machine's conditions beside a measured rate, read through the
 * platform's own power and accelerator registries.
 */

#include "Conditions.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
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
