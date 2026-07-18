#pragma once
#include "CanvasSceneBackend.h"
#include <memory>

class QRhi;

/**
 * Returns a backend that draws via Skia's Graphite (Metal) canvas instead of
 * QCanvasPainter, or null if this build doesn't have
 * SPELLCIRCLE_ENABLE_SKIA_CANVAS compiled in, `rhi` isn't Metal-backed, or
 * GPU context creation failed. Callers never need to know which case they're
 * in beyond an ordinary null check.
 */
std::unique_ptr<CanvasSceneBackend> createSkiaSceneBackend(QRhi *rhi);
