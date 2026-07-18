// Graphics-API-independent half of SkiaGraphiteContext. The static
// create() factory is per-API: exactly one of SkiaGraphiteContextMetal.mm
// or SkiaGraphiteContextVulkan.cpp is compiled into a given build (see
// CMakeLists.txt), each defining create() for the QRhi backend it serves
// and returning null for every other backend.

#include "SkiaGraphiteContext.h"

#include <gpu/graphite/Context.h>
#include <gpu/graphite/Recorder.h>

SkiaGraphiteContext::SkiaGraphiteContext(
    std::unique_ptr<skgpu::graphite::Context> context,
    std::unique_ptr<skgpu::graphite::Recorder> recorder)
    : m_context(std::move(context)), m_recorder(std::move(recorder)) {}

SkiaGraphiteContext::~SkiaGraphiteContext() = default;
