#include <cstdio>

#include "Grab.h"
#include "Servers.h"

namespace seer::texture {
int listPublications() {
  std::fprintf(stderr, "Texture reception is available on macOS with Metal.\n");
  return 4;
}
int runGrab(const Arguments&) { return listPublications(); }
}  // namespace seer::texture
