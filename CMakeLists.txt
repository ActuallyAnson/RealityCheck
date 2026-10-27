cmake_minimum_required(VERSION 3.16)
project(RealityCheck LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Determinism: no fast-math, no FP contraction (fused multiply-add changes results across targets).
add_compile_options(-Wall -Wextra -fno-fast-math -ffp-contract=off)

add_library(rc_core
  src/sim/scenario.cpp
  src/sim/world.cpp
  src/rules/checker.cpp
  src/tools/render.cpp
  src/tools/tools.cpp
  src/shrink/shrink.cpp
  src/baseline/random_baseline.cpp
  src/model/model.cpp
  src/agent/agent.cpp
  src/viewer/view_export.cpp
)
target_include_directories(rc_core PUBLIC src)

add_executable(rc_tool src/tools/rc_tool_main.cpp)
target_link_libraries(rc_tool rc_core)

add_executable(rc_agent src/agent/agent_main.cpp)
target_link_libraries(rc_agent rc_core)

add_executable(rc_eval src/eval/eval_main.cpp)
target_link_libraries(rc_eval rc_core)

enable_testing()
add_executable(rc_tests tests/test_main.cpp)
target_link_libraries(rc_tests rc_core)
add_test(NAME rc_tests COMMAND rc_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(rc_demo src/demo/demo_main.cpp)
