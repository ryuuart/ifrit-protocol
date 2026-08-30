/** @file
 * The Metal driver as Ultralight sees it: pipeline states compiled from
 * the embedded shader source, textures, render buffers and geometry
 * kept by Ultralight's ids, and the accumulated command list executed
 * by flush() as one command buffer of render passes.
 */

#import <Metal/Metal.h>

#include "MetalDriver.h"
#include "MetalDriverState.h"
#include "ShaderSource.h"

#include <Ultralight/Matrix.h>

#include <simd/simd.h>

#include <cstdio>
#include <cstring>

namespace sigil::scry {

namespace {

// Mirrors the Uniforms constant buffer in UltralightShaders.metal
// (State = [snap-enabled, screen-width, screen-height, screen-scale]).
struct Uniforms {
  simd::float4 State;
  simd::float4x4 Transform;
  simd::float4 Scalar4[2];
  simd::float4 Vector[8];
  unsigned int ClipSize;
  simd::float4x4 Clip[8];
};

simd::float4x4 toSimd(const ultralight::Matrix4x4 &m) {
  // Matrix4x4::data is column-major: each run of 4 floats is one column.
  return simd_matrix(simd_make_float4(m.data[0], m.data[1], m.data[2], m.data[3]),
                     simd_make_float4(m.data[4], m.data[5], m.data[6], m.data[7]),
                     simd_make_float4(m.data[8], m.data[9], m.data[10], m.data[11]),
                     simd_make_float4(m.data[12], m.data[13], m.data[14], m.data[15]));
}

ultralight::Matrix4x4 projectedTransform(const ultralight::GPUState &state) {
  // GPUState is a packed struct, so its transform sits at a 1-byte-aligned
  // address; the floats are read through an aligned copy because binding
  // a reference to the packed member is undefined for a 4-byte-aligned
  // type.
  ultralight::Matrix4x4 aligned;
  std::memcpy(&aligned, &state.transform, sizeof(aligned));
  ultralight::Matrix transform;
  transform.Set(aligned);
  ultralight::Matrix result;
  // No y-flip: Ultralight's texture convention already matches Metal's
  // top-left origin (flipping here double-flips the composited page).
  result.SetOrthographicProjection(state.viewport_width, state.viewport_height,
                                   /*flip_y=*/false);
  result.Transform(transform);
  return result.GetMatrix4x4();
}

}  // namespace

std::unique_ptr<MetalDriver> MetalDriver::create(sigil::skia::GpuDevice &device,
                                                 sigil::skia::GraphiteContext &graphite) {
  auto state = std::make_unique<State>();
  state->gpuDevice = &device;
  state->graphite = &graphite;
  state->device = (__bridge id<MTLDevice>)device.native().mtlDevice;
  state->queue = (__bridge id<MTLCommandQueue>)device.native().mtlCommandQueue;
  if (!state->device || !state->queue) return nullptr;

  NSError *error = nil;
  NSString *shaderSource = [NSString stringWithUTF8String:kUltralightShaderSource];
  if (!shaderSource) return nullptr;
  id<MTLLibrary> library = [state->device newLibraryWithSource:shaderSource
                                                       options:nil
                                                         error:&error];
  if (!library) {
    std::fprintf(stderr, "[SigilScry:error] Ultralight shader compile: %s\n",
                 error.localizedDescription.UTF8String);
    return nullptr;
  }

  struct {
    NSString *vertex;
    NSString *fragment;
  } entryPoints[2] = {
      {@"vertexShader", @"fragmentShader"},          // ShaderType::Fill
      {@"pathVertexShader", @"pathFragmentShader"},  // ShaderType::FillPath
  };

  for (int shader = 0; shader < 2; ++shader) {
    id<MTLFunction> vertexFn = [library newFunctionWithName:entryPoints[shader].vertex];
    id<MTLFunction> fragmentFn = [library newFunctionWithName:entryPoints[shader].fragment];
    for (int blend = 0; blend < 2; ++blend) {
      MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
      desc.vertexFunction = vertexFn;
      desc.fragmentFunction = fragmentFn;
      MTLRenderPipelineColorAttachmentDescriptor *color = desc.colorAttachments[0];
      color.pixelFormat = MTLPixelFormatBGRA8Unorm;
      color.blendingEnabled = blend != 0;
      // Ultralight emits premultiplied-alpha colour, and the destination is
      // itself composited later, so alpha accumulates with its own factors
      // rather than following the RGB ones. Changing either pair makes pages
      // with transparency composite wrong against whatever draws them.
      color.rgbBlendOperation = MTLBlendOperationAdd;
      color.alphaBlendOperation = MTLBlendOperationAdd;
      color.sourceRGBBlendFactor = MTLBlendFactorOne;
      color.sourceAlphaBlendFactor = MTLBlendFactorOneMinusDestinationAlpha;
      color.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
      color.destinationAlphaBlendFactor = MTLBlendFactorOne;

      state->pipelines[shader][blend] = [state->device newRenderPipelineStateWithDescriptor:desc
                                                                                      error:&error];
      if (!state->pipelines[shader][blend]) {
        std::fprintf(stderr, "[SigilScry:error] Ultralight pipeline: %s\n",
                     error.localizedDescription.UTF8String);
        return nullptr;
      }
    }
  }

  state->webRecorder = graphite.makeRecorder();
  if (!state->webRecorder)
    std::fprintf(stderr, "[SigilScry:warning] no Graphite recorder for the web "
                         "thread; WebImage::paint() will no-op\n");

  return std::unique_ptr<MetalDriver>(new MetalDriver(std::move(state)));
}

MetalDriver::MetalDriver(std::unique_ptr<State> state) : m_state(std::move(state)) {}

MetalDriver::~MetalDriver() = default;

uint32_t MetalDriver::NextTextureId() { return m_state->nextTextureId++; }

void MetalDriver::CreateTexture(uint32_t textureId, ultralight::RefPtr<ultralight::Bitmap> bitmap) {
  const bool isRenderTarget = bitmap->IsEmpty();
  MTLPixelFormat format = bitmap->format() == ultralight::BitmapFormat::A8_UNORM
                              ? MTLPixelFormatA8Unorm
                              : MTLPixelFormatBGRA8Unorm;
  MTLTextureDescriptor *desc = [MTLTextureDescriptor
      texture2DDescriptorWithPixelFormat:isRenderTarget ? MTLPixelFormatBGRA8Unorm : format
                                   width:bitmap->width()
                                  height:bitmap->height()
                               mipmapped:NO];
  if (isRenderTarget) {
    desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModePrivate;
  } else {
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeShared;
  }

  id<MTLTexture> texture = [m_state->device newTextureWithDescriptor:desc];
  m_state->textures[textureId] = texture;

  if (!isRenderTarget) UpdateTexture(textureId, bitmap);
}

void MetalDriver::UpdateTexture(uint32_t textureId, ultralight::RefPtr<ultralight::Bitmap> bitmap) {
  auto it = m_state->textures.find(textureId);
  if (it == m_state->textures.end()) return;
  auto pixels = bitmap->LockPixelsSafe();
  if (!pixels || !pixels.data()) return;
  [it->second replaceRegion:MTLRegionMake2D(0, 0, bitmap->width(), bitmap->height())
                mipmapLevel:0
                  withBytes:pixels.data()
                bytesPerRow:bitmap->row_bytes()];
}

void MetalDriver::DestroyTexture(uint32_t textureId) { m_state->textures.erase(textureId); }

uint32_t MetalDriver::NextRenderBufferId() { return m_state->nextRenderBufferId++; }

void MetalDriver::CreateRenderBuffer(uint32_t renderBufferId,
                                     const ultralight::RenderBuffer &buffer) {
  m_state->renderBuffers[renderBufferId] = {buffer.texture_id};
}

void MetalDriver::DestroyRenderBuffer(uint32_t renderBufferId) {
  m_state->renderBuffers.erase(renderBufferId);
  m_state->pendingClear.erase(renderBufferId);
}

uint32_t MetalDriver::NextGeometryId() { return m_state->nextGeometryId++; }

void MetalDriver::CreateGeometry(uint32_t geometryId, const ultralight::VertexBuffer &vertices,
                                 const ultralight::IndexBuffer &indices) {
  UpdateGeometry(geometryId, vertices, indices);
}

void MetalDriver::UpdateGeometry(uint32_t geometryId, const ultralight::VertexBuffer &vertices,
                                 const ultralight::IndexBuffer &indices) {
  // Fresh buffers every update: in-flight command buffers retain the old
  // ones until the GPU is done with them, so this is race-free without a
  // ring allocator.
  State::GeometryEntry entry;
  entry.vertices = [m_state->device newBufferWithBytes:vertices.data
                                                length:vertices.size
                                               options:MTLResourceStorageModeShared];
  entry.indices = [m_state->device newBufferWithBytes:indices.data
                                               length:indices.size
                                              options:MTLResourceStorageModeShared];
  m_state->geometry[geometryId] = entry;
}

void MetalDriver::DestroyGeometry(uint32_t geometryId) { m_state->geometry.erase(geometryId); }

void MetalDriver::UpdateCommandList(const ultralight::CommandList &list) {
  m_state->commands.insert(m_state->commands.end(), list.commands, list.commands + list.size);
}

std::unordered_set<uint32_t> MetalDriver::flush() {
  std::unordered_set<uint32_t> dirtyRenderBuffers;
  if (m_state->commands.empty()) return dirtyRenderBuffers;

  @autoreleasepool {
    id<MTLCommandBuffer> commandBuffer = [m_state->queue commandBuffer];
    id<MTLRenderCommandEncoder> encoder = nil;
    uint32_t encoderRenderBuffer = 0;

    auto endEncoder = [&] {
      if (encoder) {
        [encoder endEncoding];
        encoder = nil;
        encoderRenderBuffer = 0;
      }
    };

    for (const ultralight::Command &command : m_state->commands) {
      const uint32_t renderBufferId = command.gpu_state.render_buffer_id;
      auto renderBufferIt = m_state->renderBuffers.find(renderBufferId);
      if (renderBufferIt == m_state->renderBuffers.end()) continue;
      dirtyRenderBuffers.insert(renderBufferId);

      if (command.command_type == ultralight::CommandType::ClearRenderBuffer) {
        // Realized lazily as the load action of the next draw pass.
        m_state->pendingClear.insert(renderBufferId);
        if (encoderRenderBuffer == renderBufferId) endEncoder();
        continue;
      }

      // Read through an aligned copy: Command is packed (see
      // projectedTransform), so its geometry id sits at a 1-byte-aligned
      // offset and the map's lookup takes its key by reference.
      const uint32_t geometryId = command.geometry_id;
      auto textureIt = m_state->textures.find(renderBufferIt->second.textureId);
      auto geometryIt = m_state->geometry.find(geometryId);
      if (textureIt == m_state->textures.end() || geometryIt == m_state->geometry.end()) continue;
      id<MTLTexture> target = textureIt->second;

      if (!encoder || encoderRenderBuffer != renderBufferId) {
        endEncoder();
        MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = target;
        pass.colorAttachments[0].loadAction =
            m_state->pendingClear.erase(renderBufferId) ? MTLLoadActionClear : MTLLoadActionLoad;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
        encoderRenderBuffer = renderBufferId;
      }

      const ultralight::GPUState &gpu = command.gpu_state;
      const int shaderIndex = gpu.shader_type == ultralight::ShaderType::FillPath ? 1 : 0;
      [encoder setRenderPipelineState:m_state->pipelines[shaderIndex][gpu.enable_blend ? 1 : 0]];

      [encoder setViewport:MTLViewport{0.0, 0.0, (double)gpu.viewport_width,
                                       (double)gpu.viewport_height, 0.0, 1.0}];

      MTLScissorRect scissor;
      if (gpu.enable_scissor) {
        const ultralight::IntRect &r = gpu.scissor_rect;
        NSUInteger x = std::max(0, r.left);
        NSUInteger y = std::max(0, r.top);
        scissor.x = std::min(x, target.width - 1);
        scissor.y = std::min(y, target.height - 1);
        scissor.width =
            std::min((NSUInteger)std::max(0, r.right - (int)scissor.x), target.width - scissor.x);
        scissor.height =
            std::min((NSUInteger)std::max(0, r.bottom - (int)scissor.y), target.height - scissor.y);
      } else {
        scissor = {0, 0, target.width, target.height};
      }
      [encoder setScissorRect:scissor];

      Uniforms uniforms;
      std::memset(&uniforms, 0, sizeof(uniforms));
      uniforms.State =
          simd_make_float4(0.0f, (float)gpu.viewport_width, (float)gpu.viewport_height, 1.0f);
      uniforms.Transform = toSimd(projectedTransform(gpu));
      std::memcpy(uniforms.Scalar4, gpu.uniform_scalar, sizeof(float) * 8);
      std::memcpy(uniforms.Vector, gpu.uniform_vector, sizeof(uniforms.Vector));
      uniforms.ClipSize = gpu.clip_size;
      for (uint8_t i = 0; i < gpu.clip_size && i < 8; ++i) {
        // Read through an aligned copy: gpu is packed (see projectedTransform).
        ultralight::Matrix4x4 clip;
        std::memcpy(&clip, &gpu.clip[i], sizeof(clip));
        uniforms.Clip[i] = toSimd(clip);
      }

      [encoder setVertexBuffer:geometryIt->second.vertices offset:0 atIndex:0];
      [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
      [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];

      auto bindTexture = [&](uint32_t textureId, NSUInteger slot) {
        if (!textureId) return;
        auto it = m_state->textures.find(textureId);
        if (it != m_state->textures.end()) [encoder setFragmentTexture:it->second atIndex:slot];
      };
      bindTexture(gpu.texture_1_id, 0);
      bindTexture(gpu.texture_2_id, 1);

      [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                          indexCount:command.indices_count
                           indexType:MTLIndexTypeUInt32
                         indexBuffer:geometryIt->second.indices
                   indexBufferOffset:command.indices_offset * sizeof(ultralight::IndexType)];
    }

    endEncoder();
    [commandBuffer commit];
  }

  m_state->commands.clear();
  return dirtyRenderBuffers;
}

}  // namespace sigil::scry
