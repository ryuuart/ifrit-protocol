#pragma once

// One entry point per scene file in src/text/demo/. textflow_demo.cpp's
// main() calls these in the order the README describes them.

#include <textflow/FontContext.h>

#include <filesystem>

// Scene A — mixed-script paragraph reflowing around moving exclusion shapes.
void sceneExclusions(textflow::FontContext &fontContext, int frames,
                     const std::filesystem::path &outputDirectory);

// Scene B — Knuth-Plass justified paragraph with live word updates.
void sceneKnuthPlass(textflow::FontContext &fontContext, int frames,
                     const std::filesystem::path &outputDirectory);

// Scene C — freeform typography: a spiral path and a slanted line set.
void sceneFreeform(textflow::FontContext &fontContext,
                   const std::filesystem::path &outputDirectory);

// Scene D — extreme geometries: zigzag, scribble, bumpy baselines, confetti.
void sceneExtreme(textflow::FontContext &fontContext,
                  const std::filesystem::path &outputDirectory);

// Scene E — typographic options: last-line modes, hyphenation, paint
// effects, mixed fonts.
void sceneTypography(textflow::FontContext &fontContext,
                     const std::filesystem::path &outputDirectory);

// Scene F — Pretext-style glyph choreography: dispatches rain, ripple, and
// the infinite-loop marquee below.
void scenePretext(textflow::FontContext &fontContext, int frames,
                  const std::filesystem::path &outputDirectory);
void sceneRain(textflow::FontContext &fontContext, int frames,
               const std::filesystem::path &outputDirectory);
void sceneRipple(textflow::FontContext &fontContext, int frames,
                 const std::filesystem::path &outputDirectory);
void sceneLoop(textflow::FontContext &fontContext, int frames,
               const std::filesystem::path &outputDirectory);

// Scene G — script coverage (babel confetti) + OpenType features panel.
void sceneBabel(textflow::FontContext &fontContext,
                const std::filesystem::path &outputDirectory);
void sceneFeatures(textflow::FontContext &fontContext,
                   const std::filesystem::path &outputDirectory);

// Scene H — CJK typography: vertical-rl, ruby, kenten, tate-chu-yoko.
void sceneCjk(textflow::FontContext &fontContext,
              const std::filesystem::path &outputDirectory);

// Scene I — arbitrary SkPath exclusions (star / heart / donut hole).
void sceneShapes(textflow::FontContext &fontContext,
                 const std::filesystem::path &outputDirectory);

// Scene J — CJK fallback coverage across Japanese, Korean, Simplified, and
// Traditional Chinese, using Noto Sans / Serif as deliberately incomplete
// primary families.
void sceneFallback(textflow::FontContext &fontContext,
                   const std::filesystem::path &outputDirectory);

// Scene K — pathological Unicode: Arabic, Cuneiform, combining storms,
// complex clusters, bidi collisions, emoji ZWJ, and supplementary symbols.
void sceneHyperScripts(textflow::FontContext &fontContext,
                       const std::filesystem::path &outputDirectory);
