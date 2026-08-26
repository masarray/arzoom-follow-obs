cmake_minimum_required(VERSION 3.20)
project(arzoom_phase0_tests LANGUAGES CXX)

enable_testing()

function(arzoom_configure_test_target target)
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc /UNDEBUG)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
  endif()
endfunction()

add_executable(arzoom-math-test arzoom-math-test.cpp)
arzoom_configure_test_target(arzoom-math-test)
add_test(NAME arzoom-math-invariants COMMAND arzoom-math-test)

add_executable(arzoom-phase0-test arzoom-phase0-test.cpp)
arzoom_configure_test_target(arzoom-phase0-test)
add_test(NAME arzoom-phase0-traces COMMAND arzoom-phase0-test)

add_executable(arzoom-phase1-test arzoom-phase1-gate.cpp)
arzoom_configure_test_target(arzoom-phase1-test)
add_test(NAME arzoom-phase1-smart-camera COMMAND arzoom-phase1-test)

add_executable(arzoom-phase1-closeout-test arzoom-phase1-closeout-test.cpp)
arzoom_configure_test_target(arzoom-phase1-closeout-test)
add_test(NAME arzoom-phase1-closeout-stress COMMAND arzoom-phase1-closeout-test)

add_executable(arzoom-phase2-click-test arzoom-phase2-click-test.cpp)
arzoom_configure_test_target(arzoom-phase2-click-test)
add_test(NAME arzoom-phase2-click-visual COMMAND arzoom-phase2-click-test)

add_executable(arzoom-phase3-presenter-test arzoom-phase3-presenter-test.cpp)
arzoom_configure_test_target(arzoom-phase3-presenter-test)
add_test(NAME arzoom-phase3-presenter-controls COMMAND arzoom-phase3-presenter-test)

add_executable(arzoom-phase35-presentation-cursor-test arzoom-phase35-presentation-cursor-test.cpp)
arzoom_configure_test_target(arzoom-phase35-presentation-cursor-test)
add_test(NAME arzoom-phase35-presentation-cursor COMMAND arzoom-phase35-presentation-cursor-test)

add_executable(arzoom-phase35-preset-test arzoom-phase35-preset-test.cpp)
arzoom_configure_test_target(arzoom-phase35-preset-test)
add_test(NAME arzoom-phase35-cursor-presets COMMAND arzoom-phase35-preset-test)

add_executable(arzoom-phase4-scene-camera-test arzoom-phase4-scene-camera-test.cpp)
arzoom_configure_test_target(arzoom-phase4-scene-camera-test)
add_test(NAME arzoom-phase4-native-scene-camera COMMAND arzoom-phase4-scene-camera-test)

add_executable(arzoom-phase41-scene-mapping-test arzoom-phase41-scene-mapping-test.cpp)
arzoom_configure_test_target(arzoom-phase41-scene-mapping-test)
add_test(NAME arzoom-phase41-read-only-scene-mapping COMMAND arzoom-phase41-scene-mapping-test)

add_executable(arzoom-phase42-presentation-screen-resolver-test arzoom-phase42-presentation-screen-resolver-test.cpp)
arzoom_configure_test_target(arzoom-phase42-presentation-screen-resolver-test)
add_test(NAME arzoom-phase42-presentation-screen-resolver COMMAND arzoom-phase42-presentation-screen-resolver-test)

add_executable(arzoom-phase42-mapping-seam-test arzoom-phase42-mapping-seam-test.cpp)
arzoom_configure_test_target(arzoom-phase42-mapping-seam-test)
add_test(NAME arzoom-phase42-mapping-seam-extraction COMMAND arzoom-phase42-mapping-seam-test)

add_executable(arzoom-phase42-uuid-discovery-test arzoom-phase42-uuid-discovery-test.cpp)
arzoom_configure_test_target(arzoom-phase42-uuid-discovery-test)
add_test(NAME arzoom-phase42-uuid-candidate-discovery COMMAND arzoom-phase42-uuid-discovery-test)

add_executable(arzoom-phase42-presentation-screen-settings-test arzoom-phase42-presentation-screen-settings-test.cpp)
arzoom_configure_test_target(arzoom-phase42-presentation-screen-settings-test)
add_test(NAME arzoom-phase42-presentation-screen-settings COMMAND arzoom-phase42-presentation-screen-settings-test)

add_executable(arzoom-phase41-viewport-quality-test arzoom-phase41-viewport-quality-test.cpp)
arzoom_configure_test_target(arzoom-phase41-viewport-quality-test)
add_test(NAME arzoom-phase41-viewport-quality COMMAND arzoom-phase41-viewport-quality-test)

add_executable(arzoom-phase41-follow-reliability-test arzoom-phase41-follow-reliability-test.cpp)
arzoom_configure_test_target(arzoom-phase41-follow-reliability-test)
add_test(NAME arzoom-phase41-follow-reliability COMMAND arzoom-phase41-follow-reliability-test)

add_executable(arzoom-phase41-motion-quality-test arzoom-phase41-motion-quality-test.cpp)
arzoom_configure_test_target(arzoom-phase41-motion-quality-test)
add_test(NAME arzoom-phase41-motion-quality COMMAND arzoom-phase41-motion-quality-test)

add_executable(arzoom-p5-spotlight-test arzoom-p5-spotlight-test.cpp)
arzoom_configure_test_target(arzoom-p5-spotlight-test)
add_test(NAME arzoom-p5-spotlight-primitive COMMAND arzoom-p5-spotlight-test)

add_executable(arzoom-p5-shader-abi-test arzoom-p5-shader-abi-test.cpp)
arzoom_configure_test_target(arzoom-p5-shader-abi-test)
add_test(NAME arzoom-p0-p5-neutral-shader-abi COMMAND arzoom-p5-shader-abi-test)

add_executable(arzoom-cinematic-spotlight-test arzoom-cinematic-spotlight-test.cpp)
arzoom_configure_test_target(arzoom-cinematic-spotlight-test)
add_test(NAME arzoom-p5-cinematic-spotlight COMMAND arzoom-cinematic-spotlight-test)

add_executable(arzoom-spotlight-zoom-resize-test arzoom-spotlight-zoom-resize-test.cpp)
arzoom_configure_test_target(arzoom-spotlight-zoom-resize-test)
add_test(NAME arzoom-p5-zoom-resize-only COMMAND arzoom-spotlight-zoom-resize-test)

add_executable(arzoom-presentation-pass-handshake-test arzoom-presentation-pass-handshake-test.cpp)
arzoom_configure_test_target(arzoom-presentation-pass-handshake-test)
add_test(NAME arzoom-p0-presentation-pass-handshake COMMAND arzoom-presentation-pass-handshake-test)

add_executable(arzoom-cursor-sampler-guard-test arzoom-cursor-sampler-guard-test.cpp)
arzoom_configure_test_target(arzoom-cursor-sampler-guard-test)
add_test(NAME arzoom-p0-cursor-sampler-prebind COMMAND arzoom-cursor-sampler-guard-test)

add_executable(arzoom-motion-benchmark arzoom-benchmark.cpp)
arzoom_configure_test_target(arzoom-motion-benchmark)

add_executable(arzoom-smart-camera-benchmark arzoom-smart-camera-benchmark.cpp)
arzoom_configure_test_target(arzoom-smart-camera-benchmark)

add_executable(arzoom-click-benchmark arzoom-click-benchmark.cpp)
arzoom_configure_test_target(arzoom-click-benchmark)

add_executable(arzoom-presenter-benchmark arzoom-presenter-benchmark.cpp)
arzoom_configure_test_target(arzoom-presenter-benchmark)
