#pragma once

/** @file
 * WHAT EVERY DEVICE EXECUTOR HERE IS MADE OF: a compute pipeline built
 * from a kernel's words with the uniform buffer its arguments are written
 * into, a binding filled by variable name, a lane buffer that grows and
 * is then kept, and the one crossing that brings several lanes home.
 *
 * The cook, the sweep and the stamping each dispatch a kernel of their
 * own, and each needs this same plumbing around it; what differs between
 * them — which lanes a dispatch binds, how many groups it runs — stays in
 * each executor.
 */

#include <Graphics/GraphicsEngine/interface/Buffer.h>
#include <Graphics/GraphicsEngine/interface/PipelineState.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <sigilgeometry/device/Device.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <cstddef>
#include <cstdint>
#include <glm/vec4.hpp>
#include <span>
#include <vector>

namespace sigil::geometry::mesh::compute {

namespace dg = Diligent;

/** A KERNEL READY TO DISPATCH: its pipeline, the binding its resources
 *  are set through, and the uniform buffer its arguments are written
 *  into. Every variable is DYNAMIC, because each one is rebound per
 *  dispatch. */
struct Kernel {
  dg::RefCntAutoPtr<dg::IPipelineState> pipeline;
  dg::RefCntAutoPtr<dg::IShaderResourceBinding> binding;
  dg::RefCntAutoPtr<dg::IBuffer> arguments;

  /** Builds the pipeline from @p words, labelled @p name, with an
   *  argument buffer of @p argumentsSize bytes. False when the device
   *  refused any of it, which stops the dispatch rather than forming a
   *  wrong answer. */
  bool build(dg::IRenderDevice& renderDevice, const char* name,
             std::span<const uint32_t> words, size_t argumentsSize);

  /** Sets the variable the kernel calls @p name to @p object; a name the
   *  kernel does not read is passed over. */
  void bind(const char* name, dg::IDeviceObject* object);
};

/** ONE LANE BUFFER, and how many vectors it was made to hold. A dispatch
 *  formed every frame would otherwise make and destroy every one of these
 *  every frame, so a buffer already large enough is kept. */
struct GrowingLane {
  dg::RefCntAutoPtr<dg::IBuffer> buffer;
  dg::IBufferView* view = nullptr;
  size_t capacity = 0;

  /** The buffer, grown to hold at least @p count vectors. Null when the
   *  device refused it. */
  dg::IBufferView* sized(dg::IRenderDevice& device, const char* label,
                         size_t count);
};

/** @p values written into @p lane, grown to hold them first. Null when
 *  the device refused the buffer. */
dg::IBufferView* upload(device::Device& device, GrowingLane& lane,
                        const char* label,
                        const std::vector<glm::vec4>& values);

/** THE STAGING BUFFER several lanes are read back through, kept while it
 *  is large enough. */
struct Readback {
  dg::RefCntAutoPtr<dg::IBuffer> staging;
  size_t capacity = 0;

  /** The staging buffer, grown to hold at least @p bytes. Null when the
   *  device refused it. */
  dg::IBuffer* sized(dg::IRenderDevice& device, const char* label,
                     size_t bytes);
};

/** EVERY LANE IN ONE CROSSING: each of @p lanes, @p count vectors long,
 *  copied into the staging buffer, waited on once, and copied out into
 *  the matching entry of @p into. A wait per lane would cost a round trip
 *  per attribute a dispatch happens to write. */
void readBack(device::Device& device, Readback& readback, const char* label,
              std::span<dg::IBuffer* const> lanes, size_t count,
              std::span<glm::vec4* const> into);

}  // namespace sigil::geometry::mesh::compute
