#pragma once

// One entry point per scene file in src/text/demo/. weave_demo.cpp's
// main() calls these in the order the README describes them.

#include <sigilweave/FontContext.h>

#include <filesystem>

// Scene D — extreme geometries: zigzag, scribble, bumpy baselines, confetti.
void sceneExtreme(sigil::weave::FontContext &fontContext,
                  const std::filesystem::path &outputDirectory);

// Scene E — typographic options: last-line modes, hyphenation, paint
// effects, mixed fonts.
void sceneTypography(sigil::weave::FontContext &fontContext,
                     const std::filesystem::path &outputDirectory);

// Scene G — script coverage (babel confetti) + OpenType features panel.
void sceneBabel(sigil::weave::FontContext &fontContext,
                const std::filesystem::path &outputDirectory);
void sceneFeatures(sigil::weave::FontContext &fontContext,
                   const std::filesystem::path &outputDirectory);

// Scene H — CJK typography: vertical-rl, ruby, kenten, tate-chu-yoko.
void sceneCjk(sigil::weave::FontContext &fontContext,
              const std::filesystem::path &outputDirectory);

// Scene I — arbitrary SkPath exclusions (star / heart / donut hole).
void sceneShapes(sigil::weave::FontContext &fontContext,
                 const std::filesystem::path &outputDirectory);

// Scene J — CJK fallback coverage across Japanese, Korean, Simplified, and
// Traditional Chinese, using Noto Sans / Serif as deliberately incomplete
// primary families.
void sceneFallback(sigil::weave::FontContext &fontContext,
                   const std::filesystem::path &outputDirectory);

// Scene K — the standalone-pass features panel: decorations,
// text-transform, word spacing, variable axes, tab stops, and line clamp.
void sceneNewFeatures(sigil::weave::FontContext &fontContext,
                      const std::filesystem::path &outputDirectory);
