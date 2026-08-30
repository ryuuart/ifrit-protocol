// world_studies — the 3D study harness, headless:
//
//   world_studies --headless <outdir> [--study <name>]
//   world_studies --headless <outdir> --list-studies
//
// Each study is stepped from zero to its declared moment on the CPU and
// photographed, so a plate is a function of the declaration alone. That
// is what scripts/plate_ledger.py --tier world hashes.

#include "Studies.h"

int main(int argc, char* argv[]) {
  return sigil::world::testing::runStudies(sigil::world::testing::registry(),
                                           argc, argv);
}
