#include <sigilfixture/Compiled.h>

static_assert(sigil::fixture::Integer<int>);

int main() {
  const int values[] = {1, 2, 3};
  return sigil::fixture::sum(values) == 6 ? 0 : 1;
}
