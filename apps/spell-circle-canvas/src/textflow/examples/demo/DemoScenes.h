#pragma once

// One entry point per scene file in src/text/demo/. textflow_demo.cpp's
// main() calls these in the order the README describes them.

#include <textflow/FontContext.h>

#include <filesystem>

// Scene D — extreme geometries: zigzag, scribble, bumpy baselines, confetti.
void sceneExtreme(textflow::FontContext &fontContext,
                  const std::filesystem::path &outputDirectory);

// Scene E — typographic options: last-line modes, hyphenation, paint
// effects, mixed fonts.
void sceneTypography(textflow::FontContext &fontContext,
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

// Scene K — the standalone-pass features panel: decorations,
// text-transform, word spacing, variable axes, tab stops, and line clamp.
void sceneNewFeatures(textflow::FontContext &fontContext,
                      const std::filesystem::path &outputDirectory);
