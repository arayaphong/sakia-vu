cmake_minimum_required(VERSION 3.25)
project(SakiaVU CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
pkg_check_modules(FFTW3F REQUIRED IMPORTED_TARGET fftw3f)

# Box2D v3 (C library) for the physics playground overlay.
include(FetchContent)
set(BOX2D_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX2D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BOX2D_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(box2d
    GIT_REPOSITORY https://github.com/erincatto/box2d.git
    GIT_TAG v3.1.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(box2d)

set(SKIA_DIR ${CMAKE_SOURCE_DIR}/third_party/skia)
set(SKIA_LIB_DIR ${SKIA_DIR}/out/Release-x64)

add_executable(sakia-vu
    src/app/main.cpp
    src/app/AppController.cpp
    src/audio/PipeWireAudioCapture.cpp
    src/audio/FftwSpectrumAnalyzer.cpp
    src/physics/Box2dPhysicsWorld.cpp
    src/ui/SkiaMeterRenderer.cpp
    src/ui/GtkMeterWidget.cpp
)

target_include_directories(sakia-vu PRIVATE ${SKIA_DIR} src)

# Offscreen render smoke test (no GTK/audio needed).
add_executable(render-test EXCLUDE_FROM_ALL
    tools/render_test.cpp
    src/audio/FftwSpectrumAnalyzer.cpp
    src/ui/SkiaMeterRenderer.cpp
)
target_include_directories(render-test PRIVATE ${SKIA_DIR} src)
target_link_libraries(render-test PRIVATE
    PkgConfig::FFTW3F
    -Wl,--start-group
    ${SKIA_LIB_DIR}/libskia.a
    ${SKIA_LIB_DIR}/libskcms.a
    ${SKIA_LIB_DIR}/libfreetype2.a
    ${SKIA_LIB_DIR}/libharfbuzz.a
    ${SKIA_LIB_DIR}/libpng.a
    ${SKIA_LIB_DIR}/libzlib.a
    ${SKIA_LIB_DIR}/libexpat.a
    ${SKIA_LIB_DIR}/libjpeg.a
    ${SKIA_LIB_DIR}/libwebp.a
    ${SKIA_LIB_DIR}/libwebp_sse41.a
    ${SKIA_LIB_DIR}/libwuffs.a
    -Wl,--end-group
)

# Headless physics smoke test (no GTK/Skia/audio needed).
add_executable(physics-test EXCLUDE_FROM_ALL
    tools/physics_test.cpp
    src/physics/Box2dPhysicsWorld.cpp
)
target_include_directories(physics-test PRIVATE src)
target_link_libraries(physics-test PRIVATE box2d)

# Prebuilt Skia (aseprite m148) bundles its third-party deps as static libs;
# wrap in a group because the inter-archive dependencies are circular.
target_link_libraries(sakia-vu PRIVATE
    PkgConfig::GTK4
    PkgConfig::PIPEWIRE
    PkgConfig::FFTW3F
    box2d
    -Wl,--start-group
    ${SKIA_LIB_DIR}/libskia.a
    ${SKIA_LIB_DIR}/libskcms.a
    ${SKIA_LIB_DIR}/libfreetype2.a
    ${SKIA_LIB_DIR}/libharfbuzz.a
    ${SKIA_LIB_DIR}/libpng.a
    ${SKIA_LIB_DIR}/libzlib.a
    ${SKIA_LIB_DIR}/libexpat.a
    ${SKIA_LIB_DIR}/libjpeg.a
    ${SKIA_LIB_DIR}/libwebp.a
    ${SKIA_LIB_DIR}/libwebp_sse41.a
    ${SKIA_LIB_DIR}/libwuffs.a
    -Wl,--end-group
)
