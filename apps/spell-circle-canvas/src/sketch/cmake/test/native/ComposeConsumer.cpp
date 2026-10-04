#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Factories.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

int main() {
  sigil::weave::FontContext fonts(sigil::weave::ports::systemFontManager());
  sigil::motion::Engine engine;
  sigil::compose::Composer composer(engine, fonts);
  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(40, 30)) return 1;
  SkCanvas canvas(bitmap);
  composer.setSize({40, 30});
  composer.render(sigil::compose::box().width(10).height(10).fill(
      sigil::material::hexColor(0xff0000)));
  canvas.clear(SK_ColorBLACK);
  composer.draw(canvas);
  if (bitmap.getColor(5, 5) != SK_ColorRED) return 2;
  if (bitmap.getColor(20, 20) != SK_ColorBLACK) return 3;
  return 0;
}
