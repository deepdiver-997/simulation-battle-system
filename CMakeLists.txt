cmake_minimum_required(VERSION 3.18)
project(SimulationBattleSystem LANGUAGES CXX)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_SMOKE_TEST "Build smoke test executable" ON)
option(BUILD_TRAINING_CLI "Build training command-line client" ON)
option(BATTLE_FSM_VERBOSE_DEFAULT "Enable verbose FSM trace by default" OFF)

find_package(Boost REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)
# 认证数据层：离线编码效果程序(JSON)加载器依赖 nlohmann::json（header-only）。
find_path(NLOHMANN_INCLUDE_DIR nlohmann/json.hpp)

set(SIM_CORE_SOURCES
    src/primitives/battle_primitives.cpp
    src/db/official_data_repository.cpp
    src/effects/effect.cpp
    src/effects/effect_meta.cpp
    src/effects/effect_unit.cpp
    src/effects/effect_unit_parser.cpp
    src/effects/effect_unit_loader.cpp
    src/effects/core_api.cpp
    src/effects/timed_bucket.cpp
    src/effects/damage_pipeline.cpp
    src/effects/pink_damage_pipeline.cpp
    src/effects/common_trait.cpp
    src/entities/pet_factory.cpp
    src/entities/numerical-properties.cpp
    src/entities/elemental-attributes.cpp
    src/entities/skills.cpp
    src/entities/seer-robot.cpp
    src/entities/soul_mark_manager.cpp
    src/fsm/battleContext.cpp
    src/fsm/battleFsm.cpp
    src/fsm/iControlBlock.cpp
    src/utils/dynamic_library.cpp
)

add_library(sim_core STATIC ${SIM_CORE_SOURCES})
target_include_directories(sim_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(sim_core PRIVATE ${NLOHMANN_INCLUDE_DIR})
if(TARGET Boost::headers)
    target_link_libraries(sim_core PUBLIC Boost::headers)
else()
    target_include_directories(sim_core PUBLIC ${Boost_INCLUDE_DIRS})
endif()
target_link_libraries(sim_core
    PUBLIC
        SQLite::SQLite3
        Threads::Threads
        ${CMAKE_DL_LIBS}
)
target_compile_definitions(sim_core PUBLIC BOOST_ERROR_CODE_HEADER_ONLY)

if(BATTLE_FSM_VERBOSE_DEFAULT)
    target_compile_definitions(sim_core PUBLIC BATTLE_FSM_VERBOSE_DEFAULT=1)
endif()

add_executable(sim_server
    src/server/server.cpp
    src/server/main.cpp
)
target_include_directories(sim_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(sim_server PRIVATE sim_core)

if(BUILD_SMOKE_TEST)
    add_executable(sim_smoke_test test/test.cpp)
    target_include_directories(sim_smoke_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(sim_smoke_test PRIVATE sim_core)

    # 场景化对战测试：GLOB 自动发现 test/scenario/scenario_*.cpp，每个文件一个独立二进制。
    # 测试用例为本地维护（不入库），公开检出时此 GLOB 为空、不生成目标，不影响其余构建。
    file(GLOB SCENARIO_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/test/scenario/scenario_*.cpp)
    list(SORT SCENARIO_SOURCES)
    foreach(scenario_src IN LISTS SCENARIO_SOURCES)
        get_filename_component(scenario_name "${scenario_src}" NAME_WE)  # scenario_023_selfdestruct_legacy
        add_executable(sim_${scenario_name} "${scenario_src}")
        target_include_directories(sim_${scenario_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_link_libraries(sim_${scenario_name} PRIVATE sim_core)
    endforeach()
endif()

if(BUILD_TRAINING_CLI)
    add_executable(sim_training_cli tools/training_cli.cpp)
    target_include_directories(sim_training_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    if(TARGET Boost::headers)
        target_link_libraries(sim_training_cli PRIVATE Boost::headers)
    else()
        target_include_directories(sim_training_cli PRIVATE ${Boost_INCLUDE_DIRS})
    endif()
    target_compile_definitions(sim_training_cli PRIVATE BOOST_ERROR_CODE_HEADER_ONLY)
    target_link_libraries(sim_training_cli PRIVATE Threads::Threads)
endif()

add_library(moves_plugin SHARED resources/moves_lib/lib_1.cpp)
target_include_directories(moves_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(moves_plugin PROPERTIES
    OUTPUT_NAME "lib_1"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/moves_lib"
)

add_library(soul_plugin SHARED resources/soul_lib/lib_1.cpp)
target_include_directories(soul_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(soul_plugin PROPERTIES
    OUTPUT_NAME "lib_1"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/soul_lib"
)

# ⚠️ 把插件部署到源码目录 —— **必须是 ALL 自定义 target，不能用 POST_BUILD**。
#
# 运行时加载的是 `resources/<lib>/liblib_1.dylib`（`PetFactory::initialize_runtime_data`
# 里 `EffectFactory::getInstance("resources/moves_lib")`），而构建产物落在
# `build/plugins/<lib>/`。POST_BUILD 只在**该 target 重新链接**时才跑：
# 一旦 make 认为 .o 是最新的（改了头但 .cpp 没变、或源码与 .o 的 mtime 落在同一秒），
# 就不链接 → **不拷贝** → `resources/` 里那份停在旧版，**运行时加载旧代码**。
# 症状极具误导性：源码是对的、`build/plugins/` 下的是新的，唯独生效的是旧的；
# 更坏的情况是插件与 core 头不一致（字段偏移错位）→ 写进错误的字段、行为随机。
# 无输出的自定义 target **每次构建都执行**，所以部署永远跟得上。
add_custom_target(moves_plugin_deploy ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/resources/moves_lib"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:moves_plugin>" "${CMAKE_CURRENT_SOURCE_DIR}/resources/moves_lib/"
    DEPENDS moves_plugin
    COMMENT "部署 moves_plugin → resources/moves_lib/liblib_1.dylib"
)

add_custom_target(soul_plugin_deploy ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/resources/soul_lib"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:soul_plugin>" "${CMAKE_CURRENT_SOURCE_DIR}/resources/soul_lib/"
    DEPENDS soul_plugin
    COMMENT "部署 soul_plugin → resources/soul_lib/liblib_1.dylib"
)
