/** @file
 * The plumbing every device executor here shares: the compute pipeline
 * and its argument buffer, the kept lane buffers, and the one readback
 * crossing.
 */

#include "ComputeKernel.h"

#include <Graphics/GraphicsEngine/interface/DeviceContext.h>

#include <cstring>
#include <string>

namespace sigil::geometry::mesh::compute {

bool Kernel::build(dg::IRenderDevice& renderDevice, const char* name,
                   std::span<const uint32_t> words, size_t argumentsSize) {
  dg::RefCntAutoPtr<dg::IShader> cs;
  dg::ShaderCreateInfo ci;
  ci.Desc.Name = name;
  ci.Desc.ShaderType = dg::SHADER_TYPE_COMPUTE;
  ci.Desc.UseCombinedTextureSamplers = true;
  ci.ByteCode = words.data();
  ci.ByteCodeSize = words.size() * sizeof(uint32_t);
  renderDevice.CreateShader(ci, &cs);
  if (!cs) return false;

  dg::ComputePipelineStateCreateInfo info;
  info.PSODesc.Name = name;
  info.PSODesc.ResourceLayout.DefaultVariableType =
      dg::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
  info.pCS = cs;
  renderDevice.CreateComputePipelineState(info, &pipeline);
  if (!pipeline) return false;
  pipeline->CreateShaderResourceBinding(&binding, true);

  const std::string argumentsName = std::string(name) + " arguments";
  dg::BufferDesc desc;
  desc.Name = argumentsName.c_str();
  desc.Size = argumentsSize;
  desc.BindFlags = dg::BIND_UNIFORM_BUFFER;
  // Written with UpdateBuffer rather than mapped: a dispatch is not
  // necessarily inside a frame, and a default buffer's write does not
  // ask to be.
  desc.Usage = dg::USAGE_DEFAULT;
  renderDevice.CreateBuffer(desc, nullptr, &arguments);
  return (bool)arguments && (bool)binding;
}

void Kernel::bind(const char* name, dg::IDeviceObject* object) {
  if (dg::IShaderResourceVariable* variable =
          binding->GetVariableByName(dg::SHADER_TYPE_COMPUTE, name))
    variable->Set(object);
}

dg::IBufferView* GrowingLane::sized(dg::IRenderDevice& device,
                                    const char* label, size_t count) {
  if (view && capacity >= count) return view;
  buffer.Release();
  view = nullptr;
  capacity = 0;
  if (count == 0) return nullptr;
  dg::BufferDesc desc;
  desc.Name = label;
  desc.Size = count * sizeof(glm::vec4);
  // Read and written by one dispatch, so every lane is one writable
  // resource in one state rather than the same memory claimed two ways.
  desc.BindFlags = dg::BIND_UNORDERED_ACCESS;
  desc.Usage = dg::USAGE_DEFAULT;
  desc.Mode = dg::BUFFER_MODE_STRUCTURED;
  desc.ElementByteStride = sizeof(glm::vec4);
  device.CreateBuffer(desc, nullptr, &buffer);
  if (!buffer) return nullptr;
  capacity = count;
  view = buffer->GetDefaultView(dg::BUFFER_VIEW_UNORDERED_ACCESS);
  return view;
}

dg::IBufferView* upload(device::Device& device, GrowingLane& lane,
                        const char* label,
                        const std::vector<glm::vec4>& values) {
  dg::IBufferView* view =
      lane.sized(*device.renderDevice(), label, values.size());
  if (!view || values.empty()) return view;
  device.context()->UpdateBuffer(
      lane.buffer, 0, values.size() * sizeof(glm::vec4), values.data(),
      dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  return view;
}

dg::IBuffer* Readback::sized(dg::IRenderDevice& device, const char* label,
                             size_t bytes) {
  if (staging && capacity >= bytes) return staging;
  staging.Release();
  dg::BufferDesc desc;
  desc.Name = label;
  desc.Size = bytes;
  desc.Usage = dg::USAGE_STAGING;
  desc.CPUAccessFlags = dg::CPU_ACCESS_READ;
  desc.BindFlags = dg::BIND_NONE;
  device.CreateBuffer(desc, nullptr, &staging);
  capacity = staging ? bytes : 0;
  return staging;
}

void readBack(device::Device& device, Readback& readback, const char* label,
              std::span<dg::IBuffer* const> lanes, size_t count,
              std::span<glm::vec4* const> into) {
  dg::IDeviceContext* context = device.context();
  const size_t stride = count * sizeof(glm::vec4);
  const size_t wanted = stride * lanes.size();
  if (wanted == 0) return;
  dg::IBuffer* staging = readback.sized(*device.renderDevice(), label, wanted);
  if (!staging) return;

  for (size_t i = 0; i < lanes.size(); ++i)
    context->CopyBuffer(lanes[i], 0,
                        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, staging,
                        (dg::Uint64)i * (dg::Uint64)stride, (dg::Uint64)stride,
                        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->WaitForIdle();

  void* mapped = nullptr;
  context->MapBuffer(staging, dg::MAP_READ, dg::MAP_FLAG_DO_NOT_WAIT, mapped);
  if (!mapped) return;
  const auto* bytes = static_cast<const std::byte*>(mapped);
  for (size_t i = 0; i < into.size() && i < lanes.size(); ++i)
    std::memcpy(into[i], bytes + stride * i, stride);
  context->UnmapBuffer(staging, dg::MAP_READ);
}

}  // namespace sigil::geometry::mesh::compute
