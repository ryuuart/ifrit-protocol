#pragma once
#include <memory>

#include "CanvasSceneBackend.h"

class QRhi;

/**
 * Returns a backend that draws via Skia's Graphite (Metal) canvas instead of
 * QCanvasPainter, or null if `rhi` isn't Metal-backed or GPU context
 * creation failed. Callers never need to know which case they're in beyond
 * an ordinary null check.
 */
std::unique_ptr<CanvasSceneBackend> createSkiaSceneBackend(QRhi* rhi);
