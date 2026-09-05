# The engine feature: the Ultralight renderer and the web thread that
# drives it, the views whose repaints are published as immutable frames,
# and the image slots pages display native drawing through. The three
# public headers here are the library's API; the platform and gpu
# features' internal headers are reached by directory because they name
# Ultralight types and Ultralight is private to the library.
sigil_library(SigilScryEngine
  SOURCES Bringup.cpp Engine.cpp EngineImpl.h Image.cpp ImageImpl.h View.cpp
          ViewImpl.cpp ViewImpl.h WebThread.cpp
  HEADERS ../Scry.h WebEngine.h WebImage.h WebView.h
  INCLUDE_PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../platform
                  ${CMAKE_CURRENT_LIST_DIR}/../gpu
  PUBLIC SigilScryPlatform SigilScryGpu SigilSkiaGraphite SigilCoreHardware
         unofficial::skia::skia
  PRIVATE Ultralight::Ultralight Ultralight::AppCore Boost::unordered)

# Ultralight allows one Renderer per process, so the CPU-mode and the
# GPU-mode engines are tested in separate binaries. Each links the
# library it exercises.
sigil_test(scry_engine_test
  SOURCES test/EngineTest.cpp
  LIBRARIES SigilScryEngine)
ultralight_copy_resources(scry_engine_test)

# The integration benchmark; run `scry_engine_bench` for the CPU engine
# and `scry_engine_bench --gpu` for the GPU engine (one renderer per
# process, so the two modes are separate runs; the ledger runs the CPU
# mode).
if(APPLE)
  set(SIGIL_SCRY_ENGINE_BENCH_GPU_SOURCES bench/BenchGpuMetal.mm)
else()
  set(SIGIL_SCRY_ENGINE_BENCH_GPU_SOURCES bench/BenchGpuStub.cpp)
endif()
sigil_bench(scry_engine_bench
  SOURCES bench/EngineBench.cpp bench/BenchGpu.h
          ${SIGIL_SCRY_ENGINE_BENCH_GPU_SOURCES}
  LIBRARIES SigilScryEngine $<$<BOOL:${APPLE}>:${METAL_FRAMEWORK}>
  ARC)
ultralight_copy_resources(scry_engine_bench)

# GPU-mode engine: Ultralight's Metal driver feeding texture-backed
# SkImages into the Graphite context the test shares with the engine.
# Every case here needs a device, so on a machine without one the binary
# checks nothing; the label makes that visible as a lane not run rather
# than as a lane that passed.
if(APPLE)
  sigil_test(scry_engine_gpu_test
    SOURCES test/EngineGpuTest.mm
    LIBRARIES SigilScryEngine ${METAL_FRAMEWORK}
    LABELS gpu)
  ultralight_copy_resources(scry_engine_gpu_test)
endif()
