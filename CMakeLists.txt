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
  src/sim/cartpole.cpp
  src/sim/traffic.cpp
  src/sim/generic.cpp
  src/rules/checker.cpp
  src/rules/metamorphic.cpp
  src/tools/render.cpp
  src/tools/tools.cpp
  src/shrink/shrink.cpp
  src/baseline/random_baseline.cpp
  src/model/model.cpp
  src/agent/agent.cpp
  src/viewer/view_export.cpp
  src/spec/spec.cpp
  src/spec/miner.cpp
  src/external/exec_sim.cpp
)
target_include_directories(rc_core PUBLIC src)

add_executable(rc_tool src/tools/rc_tool_main.cpp)
target_link_libraries(rc_tool rc_core)

add_executable(rc_agent src/agent/agent_main.cpp)
target_link_libraries(rc_agent rc_core)

find_path(ODEINT_INC boost/numeric/odeint.hpp HINTS /opt/homebrew/include /usr/local/include)
if(ODEINT_INC)  # optional: an orbit simulator built on unmodified Boost.odeint (examples/external/odeint_orbit.cpp)
  add_executable(odeint_orbit examples/external/odeint_orbit.cpp)
  target_include_directories(odeint_orbit PRIVATE src ${ODEINT_INC})
endif()
add_executable(naive_orbit examples/external/naive_orbit.cpp)
target_include_directories(naive_orbit PRIVATE src)

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

# Engines: BYO-simulator over a pipe (always), plus optionally unmodified Box2D v3 (`brew install box2d`).
add_library(rc_engines src/external/engine.cpp)
target_link_libraries(rc_engines rc_core)
find_library(BOX2D_LIB box2d HINTS /opt/homebrew/lib /usr/local/lib)
find_path(BOX2D_INC box2d/box2d.h HINTS /opt/homebrew/include /usr/local/include)
if(BOX2D_LIB AND BOX2D_INC)
  target_sources(rc_engines PRIVATE src/external/box2d_sim.cpp)
  target_include_directories(rc_engines PRIVATE ${BOX2D_INC})
  target_compile_definitions(rc_engines PUBLIC RC_HAVE_BOX2D)
  target_link_libraries(rc_engines ${BOX2D_LIB})
else()
  message(STATUS "Box2D not found: box2d engines not built")
endif()
add_executable(rc_hunt src/external/hunt_main.cpp)
target_link_libraries(rc_hunt rc_engines)
add_executable(rc_repro src/external/repro_main.cpp)
target_link_libraries(rc_repro rc_engines)
target_link_libraries(rc_agent rc_engines)
add_executable(rc_diff src/external/diff_main.cpp)
target_link_libraries(rc_diff rc_engines)
target_link_libraries(rc_tests rc_engines)
add_executable(rc_spec src/spec/spec_main.cpp)
target_link_libraries(rc_spec rc_engines)
add_executable(rc_climb src/eval/climb_main.cpp)
target_link_libraries(rc_climb rc_core)
add_executable(rc_triage src/eval/triage_main.cpp)
target_link_libraries(rc_triage rc_core)
add_library(rc_api src/api/api.cpp)
target_link_libraries(rc_api rc_engines)
add_executable(rc_api_server src/api/api_main.cpp)
target_link_libraries(rc_api_server rc_api Threads::Threads)
target_link_libraries(rc_tests rc_api)
add_executable(rc_corpus src/api/corpus_main.cpp)
target_link_libraries(rc_corpus rc_api)
add_executable(rc_mutgen src/eval/mutgen_main.cpp)
target_link_libraries(rc_mutgen rc_core)
add_executable(mut_eval src/eval/mut_eval_main.cpp)
target_link_libraries(mut_eval rc_core)
add_executable(rc_mine src/spec/mine_main.cpp)
target_link_libraries(rc_mine rc_engines)
add_executable(rc_redteam src/eval/redteam_main.cpp)
target_link_libraries(rc_redteam rc_core)
add_executable(rc_minescore src/eval/minescore_main.cpp)
target_link_libraries(rc_minescore rc_core)
add_executable(rc_mutscore src/eval/mutscore_main.cpp)
target_link_libraries(rc_mutscore rc_core)
add_executable(rc_fuzz src/eval/fuzz_main.cpp)
target_link_libraries(rc_fuzz rc_api)
add_executable(generic_tank examples/generic/tank.cpp)
target_include_directories(generic_tank PRIVATE src)
add_executable(rc src/rc_main.cpp)
target_link_libraries(rc rc_engines)
add_executable(generic_tank_injected examples/generic/tank_injected.cpp)
target_include_directories(generic_tank_injected PRIVATE src)
add_executable(rc_inject src/eval/inject_main.cpp)
target_link_libraries(rc_inject rc_core)
add_executable(rc_repair src/eval/repair_main.cpp)
target_link_libraries(rc_repair rc_core)

add_executable(rc_strictcheck src/eval/strictcheck_main.cpp)
target_link_libraries(rc_strictcheck rc_core)

add_executable(rc_plant src/eval/plant_main.cpp)
target_link_libraries(rc_plant rc_core)

add_executable(rc_stepdump src/eval/stepdump_main.cpp)
target_link_libraries(rc_stepdump rc_core)
add_executable(rc_refjudge src/eval/refjudge_main.cpp)
add_executable(rc_refgen src/eval/refgen_main.cpp)
target_link_libraries(rc_refgen rc_core)
add_executable(rc_refscore src/eval/refscore_main.cpp)
target_link_libraries(rc_refscore rc_core)

add_executable(rc_randbase src/eval/randbase_main.cpp)
target_link_libraries(rc_randbase rc_core)

add_executable(rc_gap src/eval/gap_main.cpp)
target_link_libraries(rc_gap rc_core)
