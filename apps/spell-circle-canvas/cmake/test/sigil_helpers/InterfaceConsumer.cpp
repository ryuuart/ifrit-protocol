#include <sigilfixture/Interface.h>

int main() {
  const int values[] = {7};
  return sigil::fixture::first<int>(values) == 7 ? 0 : 1;
}
