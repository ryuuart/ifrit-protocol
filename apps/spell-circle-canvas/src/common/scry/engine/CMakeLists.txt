# The engine feature: the Ultralight renderer and the web thread that
# drives it, the views whose repaints are published as immutable frames,
# and the image slots pages display native drawing through. The three
# public headers here are the library's API; the platform and gpu
# features' internal headers are reached by directory because they name
# Ultralight types and Ultralight is private to the library.
add_library(SigilScryEngine STATIC
  Bringup.cpp
  Engine.cpp
  EngineImpl.h
  Image.cpp
  ImageImpl.h
  View.cpp
  ViewImpl.cpp
  ViewImpl.h
  WebThread.cpp
  ${SIGIL_SCRY_INCLUDE_DIR}/sigilscry/Scry.h
  ${SIGIL_SCRY_INCLUDE_DIR}/sigilscry/engine/WebEngine.h
  ${SIGIL_SCRY_INCLUDE_DIR}/sigilscry/engine/WebImage.h
  ${SIGIL_SCRY_INCLUDE_DIR}/sigilscry/engine/WebView.h
)
target_compile_features(SigilScryEngine PUBLIC cxx_std_20)
target_include_directories(SigilScryEngine
  PUBLIC ${SIGIL_SCRY_INCLUDE_DIR}
  PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../platform ${CMAKE_CURRENT_LIST_DIR}/../gpu)
target_link_libraries(SigilScryEngine
  PUBLIC SigilScryPlatform SigilScryGpu SigilSkia unofficial::skia::skia
  PRIVATE Ultralight::Ultralight Ultralight::AppCore)

# Ultralight allows one Renderer per process, so the CPU-mode and the
# GPU-mode engines are tested in separate binaries. Each links the
# library it exercises.
add_executable(scry_engine_test test/EngineTest.cpp)
target_link_libraries(scry_engine_test PRIVATE SigilScryEngine GTest::gtest_main)
ultralight_copy_resources(scry_engine_test)
add_test(NAME scry_engine_test COMMAND scry_engine_test)

# The integration benchmark; run `scry_engine_bench` for the CPU engine
# and `scry_engine_bench --gpu` for the GPU engine (one renderer per
# process, so the two modes are separate runs; the ledger runs the CPU
# mode). Not a test: it builds through the `benches` target and runs
# through scripts/bench_ledger.py.
if(APPLE)
  set(SIGIL_SCRY_ENGINE_BENCH_GPU_SOURCES bench/BenchGpuMetal.mm)
else()
  set(SIGIL_SCRY_ENGINE_BENCH_GPU_SOURCES bench/BenchGpuStub.cpp)
endif()
add_executable(scry_engine_bench
  bench/EngineBench.cpp
  bench/BenchGpu.h
  ${SIGIL_SCRY_ENGINE_BENCH_GPU_SOURCES}
)
target_link_libraries(scry_engine_bench PRIVATE SigilScryEngine benchmark::benchmark)
if(APPLE)
  set_source_files_properties(bench/BenchGpuMetal.mm PROPERTIES
    COMPILE_OPTIONS -fobjc-arc)
  target_link_libraries(scry_engine_bench PRIVATE ${METAL_FRAMEWORK})
endif()
ultralight_copy_resources(scry_engine_bench)
add_dependencies(benches scry_engine_bench)

# GPU-mode engine: Ultralight's Metal driver feeding texture-backed
# SkImages into the Graphite context the test shares with the engine.
if(APPLE)
  add_executable(scry_engine_gpu_test test/EngineGpuTest.mm)
  # The library's own test directory holds the device, the Graphite
  # context and the asynchronous read every GPU test here shares.
  target_include_directories(scry_engine_gpu_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../test)
  target_link_libraries(scry_engine_gpu_test PRIVATE
    SigilScryEngine GTest::gtest_main ${METAL_FRAMEWORK})
  ultralight_copy_resources(scry_engine_gpu_test)
  add_test(NAME scry_engine_gpu_test COMMAND scry_engine_gpu_test)
  # Every case here needs a device, so on a machine without one the
  # binary checks nothing. The label is what makes that visible as a
  # lane not run rather than as a lane that passed.
  set_tests_properties(scry_engine_gpu_test PROPERTIES LABELS gpu)
endif()
