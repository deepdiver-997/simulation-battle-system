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
    src/entities/suit_manager.cpp
    src/entities/suit.cpp
    src/fsm/battleContext.cpp
    src/fsm/battleFsm.cpp
    src/fsm/battle_mutate.cpp
    src/utils/dynamic_library.cpp
)

add_library(sim_core STATIC ${SIM_CORE_SOURCES})
target_include_directories(sim_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(sim_core PRIVATE ${NLOHMANN_INCLUDE_DIR})
target_link_libraries(sim_core
    PUBLIC
        SQLite::SQLite3
        Threads::Threads
        ${CMAKE_DL_LIBS}
)

if(BATTLE_FSM_VERBOSE_DEFAULT)
    target_compile_definitions(sim_core PUBLIC BATTLE_FSM_VERBOSE_DEFAULT=1)
endif()

# net 层：原生事件循环（macOS/BSD = kqueue，Linux = epoll）+ 长度前缀成帧。
# 不依赖 Boost：原 asio 用法只有 accept / async_read_some / async_write / thread_pool::post，
# 自持一个 reactor 的维护成本远低于让整个项目背 Boost 依赖。
# Windows(IOCP) 不在本轮范围，见 docs_local 的网络层设计文档。
set(SIM_NET_SOURCES
    src/net/poller_platform.cpp
    src/net/reactor.cpp
)
add_library(sim_net STATIC ${SIM_NET_SOURCES})
target_include_directories(sim_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(sim_net PUBLIC Threads::Threads)

# server 层：一对局一 Room（同时是 FSM 的 IControlBlock 实现）+ 每连接一 Session。
# 座位数决定 solo（一连接驱动双方）还是 pvp（两连接一局），对局层代码完全共用。
# 注意：不含 main.cpp —— 端到端测试会复用这份列表，两个 main 会冲突。
set(SIM_SERVER_SOURCES
    src/server/service.cpp
    src/server/room.cpp
    src/server/session.cpp
    src/server/battle_init.cpp
    src/server/legal_actions.cpp
    src/server/output_json.cpp
)
add_executable(sim_server src/server/main.cpp ${SIM_SERVER_SOURCES})
target_include_directories(sim_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${NLOHMANN_INCLUDE_DIR}
)
target_link_libraries(sim_server PRIVATE sim_core sim_net)

# net 层验证：成帧 / 并发回显 / 部分 IO / 背压 / 关闭语义 / 定时器 / fd 泄漏。
# test/ 是本地维护目录（.gitignore 排除），公开检出里不存在 → 用 EXISTS 守卫，
# 否则 add_executable 会因为源文件缺失在 configure 阶段直接失败。
if(NOT WIN32 AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/test_net_reactor.cpp)
    add_executable(sim_net_test test/test_net_reactor.cpp)
    target_include_directories(sim_net_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(sim_net_test PRIVATE sim_net)
endif()

# 服务端端到端：真起 reactor + BattleService，真用 socket 打一局。
# 守的是"各层单测全绿、合起来不能用"那类问题（字节序、生命周期）。
# 复用 sim_server 的源文件（不是复制），保证测的就是线上那份实现。
if(NOT WIN32 AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/test_server_e2e.cpp)
    add_executable(sim_server_e2e_test test/test_server_e2e.cpp ${SIM_SERVER_SOURCES})
    target_include_directories(sim_server_e2e_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${NLOHMANN_INCLUDE_DIR}
    )
    target_link_libraries(sim_server_e2e_test PRIVATE sim_core sim_net)
endif()

# 注意：sim_smoke_test 同样依赖 test/ 下的本地文件。原先这里没有 EXISTS 守卫，
# 而公开检出不含 test/ 目录 → 公开面 configure 会因"Cannot find source file: test/test.cpp" 失败。
if(BUILD_SMOKE_TEST AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/test.cpp)
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
        target_include_directories(sim_${scenario_name} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${NLOHMANN_INCLUDE_DIR}   # 场景按需 include src/server/*.cpp（JSON 解码复用）时需要
        )
        target_link_libraries(sim_${scenario_name} PRIVATE sim_core)
    endforeach()
endif()

if(BUILD_TRAINING_CLI)
    # 命令行联调客户端：原生阻塞 socket，不再依赖 Boost。
    add_executable(sim_training_cli tools/training_cli.cpp)
    target_include_directories(sim_training_cli PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${NLOHMANN_INCLUDE_DIR}
    )
    target_link_libraries(sim_training_cli PRIVATE sim_net Threads::Threads)
endif()

# ── 对战 fuzz（test/fuzz，本地维护目录 → EXISTS 守卫同 test/ 段）────────────
# 用法见 test/fuzz/README.md。⚠️ SBS_FUZZ_SANITIZERS 必须用在独立 build 目录：
# 插件 deploy 会把带 sanitizer 的 lib_1 拷进源码树 resources/*_lib/，普通 build
# 运行时会加载它 → 混用必崩（恢复方法见 README）。
option(SBS_FUZZ_SANITIZERS "Fuzz 构建变体：本目录所有目标加 ASan+UBSan" OFF)
if(SBS_FUZZ_SANITIZERS)
    # -fno-sanitize-recover：UBSan 报错即 abort —— fuzz 的 oracle 只认"崩"。
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined
                        -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/fuzz/fuzz_battle.cpp)
    add_executable(sim_fuzz_battle
        test/fuzz/fuzz_battle.cpp
        src/server/legal_actions.cpp   # compute_legal_actions —— fuzz 驱动的合法动作源
    )
    target_include_directories(sim_fuzz_battle PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(sim_fuzz_battle PRIVATE sim_core)
endif()

# ── 插件段：源码为**本地维护**（.gitignore 排除，见 docs_local/多agent协作工作流.md），
#    公开检出里不存在 → **必须用 EXISTS 守卫**，否则 `add_library` 会因为源文件缺失在
#    configure 阶段直接失败（CLAUDE.md 的口径是"公开检出不影响构建"；上面 test/ 段同款）。
#
#    ⚠️ 守卫挑的判据是"**家族拆分后**才存在的文件"（2026-09-17 的 lib_1 单体拆分）：
#    只有"旧的单文件插件"的检出（如原仓库的 workspace —— 它只有 resources/*/lib_1.cpp 单体）
#    也会正确跳过整段，因为单文件登记与家族拆分登记互不兼容，套用哪一套都不对。
#    ⚠️ 不要只判 `resources/moves_lib/lib_1.cpp` 存在就进 —— 旧单体同名文件会让守卫误判。
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/moves_lib/stat_dispel.cpp
   AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/soul_lib/legacy_guards.cpp
   AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/suit_lib/lib_1.cpp)
# moves_plugin 按语义家族分文件（2026-09-17 从 lib_1.cpp 单体拆分；common.h 为共享助手）。
# 新增效果先选家族文件；跨家族的新助手进 common.h（inline）。
# 2026-09-26 起源文件 GLOB 化（CONFIGURE_DEPENDS）：新增 *.cpp 不再需要登记本文件——
# 各机制分支因此不再产生 CMakeLists 合并冲突（场景 GLOB 同款策略）。家族说明看各 cpp 头注释。
file(GLOB MOVES_PLUGIN_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/moves_lib/*.cpp)
list(SORT MOVES_PLUGIN_SOURCES)
add_library(moves_plugin SHARED ${MOVES_PLUGIN_SOURCES})
target_include_directories(moves_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(moves_plugin PROPERTIES
    OUTPUT_NAME "lib_1"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/moves_lib"
)

# soul_plugin 按机制家族分文件（2026-09-17 从 lib_1.cpp 单体拆分；common.h 为共享助手）。
# 新增魂印先选家族文件；跨家族的新助手进 common.h（inline）。
# 拆分不变式：SOUL_MARK + SOUL_PROGRAM 注册总数只增不改，全量场景回归守护。
# 2026-09-26 起源文件 GLOB 化（同 moves_plugin）——新增 *.cpp 免登记、合并零冲突。
file(GLOB SOUL_PLUGIN_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/soul_lib/*.cpp)
list(SORT SOUL_PLUGIN_SOURCES)
add_library(soul_plugin SHARED ${SOUL_PLUGIN_SOURCES})
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

# suit_plugin（套装线，2026-09-19）：套装效果动态库，入口符号 suit_plugin_register
# （独立于 moves/soul 的 plugin_register，见 include/plugin/suit_reg.h）。
# 加载方 SuitManager::getInstance("resources/suit_lib")（pet_factory.cpp 初始化）。
# 2026-09-26 GLOB 化（同 moves/soul）。
file(GLOB SUIT_PLUGIN_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/suit_lib/*.cpp)
list(SORT SUIT_PLUGIN_SOURCES)
add_library(suit_plugin SHARED ${SUIT_PLUGIN_SOURCES})
target_include_directories(suit_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(suit_plugin PROPERTIES
    OUTPUT_NAME "lib_1"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/suit_lib"
)

add_custom_target(suit_plugin_deploy ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/resources/suit_lib"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:suit_plugin>" "${CMAKE_CURRENT_SOURCE_DIR}/resources/suit_lib/"
    DEPENDS suit_plugin
    COMMENT "部署 suit_plugin → resources/suit_lib/liblib_1.dylib"
)
endif()
