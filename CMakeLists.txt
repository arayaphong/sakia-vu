cmake_minimum_required(VERSION 3.25)
project(SakiaVU CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
pkg_check_modules(FFTW3F REQUIRED IMPORTED_TARGET fftw3f)

set(SKIA_DIR ${CMAKE_SOURCE_DIR}/third_party/skia)
set(SKIA_LIB_DIR ${SKIA_DIR}/out/Release-x64)

add_executable(sakia-vu
    src/main.cpp
    src/AudioCapture.cpp
    src/SpectrumAnalyzer.cpp
    src/MeterRenderer.cpp
    src/MeterWidget.cpp
)

target_include_directories(sakia-vu PRIVATE ${SKIA_DIR})

# Offscreen render smoke test (no GTK/audio needed).
add_executable(render-test EXCLUDE_FROM_ALL
    tools/render_test.cpp
    src/SpectrumAnalyzer.cpp
    src/MeterRenderer.cpp
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

# Prebuilt Skia (aseprite m148) bundles its third-party deps as static libs;
# wrap in a group because the inter-archive dependencies are circular.
target_link_libraries(sakia-vu PRIVATE
    PkgConfig::GTK4
    PkgConfig::PIPEWIRE
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
