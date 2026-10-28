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
  src/sim/orbit.cpp
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

add_executable(rc_orbit_cal src/eval/orbit_cal_main.cpp)
target_link_libraries(rc_orbit_cal rc_core)

add_executable(rc_eval src/eval/eval_main.cpp)
target_link_libraries(rc_eval rc_core)

enable_testing()
add_executable(rc_tests tests/test_main.cpp)
target_link_libraries(rc_tests rc_core)
add_test(NAME rc_tests COMMAND rc_tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

find_package(Threads REQUIRED)
add_executable(rc_demo src/demo/demo_main.cpp)
target_link_libraries(rc_demo rc_core Threads::Threads)

# Optional: test an unmodified outside engine (Box2D v3, e.g. `brew install box2d`).
find_library(BOX2D_LIB box2d HINTS /opt/homebrew/lib /usr/local/lib)
find_path(BOX2D_INC box2d/box2d.h HINTS /opt/homebrew/include /usr/local/include)
if(BOX2D_LIB AND BOX2D_INC)
  add_executable(rc_box2d src/external/box2d_main.cpp src/external/box2d_sim.cpp)
  target_include_directories(rc_box2d PRIVATE ${BOX2D_INC})
  target_link_libraries(rc_box2d rc_core ${BOX2D_LIB})
  target_compile_definitions(rc_agent PRIVATE RC_HAVE_BOX2D)
  target_include_directories(rc_agent PRIVATE ${BOX2D_INC} src)
  target_sources(rc_agent PRIVATE src/external/box2d_sim.cpp)
  target_link_libraries(rc_agent ${BOX2D_LIB})
else()
  message(STATUS "Box2D not found: rc_box2d not built")
endif()
