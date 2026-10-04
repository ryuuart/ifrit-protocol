#include <sigilcompose/typography/TextFx.h>

int main() {
  const auto effect = sigil::compose::textFx::scramble(U"AB", 3);
  return effect ? 0 : 1;
}
