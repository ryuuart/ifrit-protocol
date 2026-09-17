#include "Servers.h"

#import <Foundation/Foundation.h>
#include <sigilio/publish/Subscription.h>
#include <cstdio>

namespace seer::texture {

void turnRunLoop(double seconds) {
  NSDate* until = [NSDate dateWithTimeIntervalSinceNow:seconds];
  while ([until timeIntervalSinceNow] > 0) {
    if (![[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:until]) {
      [NSThread sleepUntilDate:until];
      return;
    }
  }
}

int listPublications() {
  @autoreleasepool {
    sigil::io::publish::publications();
    turnRunLoop(0.5);
    const auto offered = sigil::io::publish::publications();
    for (const auto& publication : offered)
      std::printf("%s\t%s\n", publication.name.c_str(), publication.application.c_str());
    if (offered.empty()) std::fprintf(stderr, "No textures are being published on this machine.\n");
    return 0;
  }
}

}  // namespace seer::texture
