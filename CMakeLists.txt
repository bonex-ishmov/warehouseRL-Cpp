cmake_minimum_required(VERSION 3.24)
project(WarehouseRL VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo de compilación" FORCE)
endif()

option(WRL_BUILD_AGENT  "Compilar el agente SAC (requiere LibTorch)" ON)
option(WRL_BUILD_VIEWER "Compilar el visor SFML y la interfaz gráfica" ON)
option(WRL_BUILD_TESTS  "Compilar las pruebas GoogleTest" ON)
option(WRL_NATIVE       "Optimizar para la CPU de esta máquina (-march=native)" ON)
option(WRL_LTO          "Optimización en tiempo de enlace" ON)

# Opciones de compilación solo para nuestros targets (no para dependencias).
add_library(wrl_options INTERFACE)
target_compile_options(wrl_options INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow
    $<$<CONFIG:Release,RelWithDebInfo>:-O3>
    $<$<BOOL:${WRL_NATIVE}>:-march=native>)

if(WRL_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo OUTPUT _ipo_msg)
    if(_ipo)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
endif()

include(cmake/Dependencies.cmake)

add_compile_definitions(WRL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# --- Núcleo: simulación física, mundo, planificación, entorno (sin Torch ni SFML)
add_library(wrl_core STATIC
    src/core/math.cpp
    src/core/rng.cpp
    src/io/config.cpp
    src/io/metrics_logger.cpp
    src/io/trace.cpp
    src/world/warehouse_map.cpp
    src/world/occupancy_grid.cpp
    src/physics/physics_world.cpp
    src/physics/robot_model.cpp
    src/physics/dynamic_obstacles.cpp
    src/sensors/sensor_suite.cpp
    src/planning/astar.cpp
    src/planning/path.cpp
    src/planning/dwa.cpp
    src/sim/task_manager.cpp
    src/sim/task_selector.cpp
    src/sim/scenario_generator.cpp
    src/env/warehouse_env.cpp
    src/env/vec_env.cpp
    src/env/policies.cpp
    src/session/session.cpp)
target_include_directories(wrl_core PUBLIC include)
target_link_libraries(wrl_core
    PUBLIC box2d::box2d nlohmann_json::nlohmann_json Threads::Threads
    PRIVATE wrl_options)

add_executable(wrl_benchmark apps/benchmark.cpp)
target_link_libraries(wrl_benchmark PRIVATE wrl_core wrl_options)

add_executable(wrl_validate apps/validate_scenarios.cpp)
target_link_libraries(wrl_validate PRIVATE wrl_core wrl_options)

# --- Agente SAC (LibTorch)
if(WRL_BUILD_AGENT)
    include(cmake/LibTorch.cmake)
    add_library(wrl_agent STATIC
        src/agent/sac.cpp
        src/agent/replay_buffer.cpp
        src/agent/sac_policy.cpp)
    target_include_directories(wrl_agent PUBLIC include)
    target_link_libraries(wrl_agent PUBLIC wrl_core Torch::Torch PRIVATE wrl_options)
    target_compile_definitions(wrl_agent PUBLIC WRL_HAS_TORCH=1)

    add_executable(wrl_train apps/train.cpp)
    target_link_libraries(wrl_train PRIVATE wrl_agent wrl_options)

    add_executable(wrl_bench_sac apps/bench_sac.cpp)
    target_link_libraries(wrl_bench_sac PRIVATE wrl_agent wrl_options)
endif()

# --- Evaluación (con o sin agente)
add_executable(wrl_evaluate apps/evaluate.cpp)
target_link_libraries(wrl_evaluate PRIVATE wrl_core wrl_options)
if(WRL_BUILD_AGENT)
    target_link_libraries(wrl_evaluate PRIVATE wrl_agent)
endif()

# --- Visor SFML e interfaz gráfica
# Mismo reparto que en Proyecto_DQL: wrl_core (lógica, sin gráficos) <- wrl_render
# (solo dibuja) <- aplicaciones; el agente (wrl_agent) se enlaza aparte y la
# interfaz solo lo ve a través de la interfaz Policy.
if(WRL_BUILD_VIEWER)
    include(cmake/SFML.cmake)
    add_library(wrl_render STATIC src/render/renderer.cpp)
    target_include_directories(wrl_render PUBLIC include)
    target_link_libraries(wrl_render PUBLIC wrl_core SFML::Graphics PRIVATE wrl_options)

    add_executable(wrl_viewer apps/viewer.cpp)
    target_link_libraries(wrl_viewer PRIVATE wrl_render wrl_options)
    if(WRL_BUILD_AGENT)
        target_link_libraries(wrl_viewer PRIVATE wrl_agent)
    endif()

    include(cmake/ImGui.cmake)
    add_executable(wrl_gui
        apps/gui/main.cpp
        apps/gui/gui_app.cpp
        apps/gui/plots.cpp)
    target_link_libraries(wrl_gui PRIVATE wrl_render imgui_sfml wrl_options)
    if(WRL_BUILD_AGENT)
        target_link_libraries(wrl_gui PRIVATE wrl_agent)
    endif()
endif()

if(WRL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
