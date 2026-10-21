#!/usr/bin/env python3
"""enable_examples.py —— 在**已 clone 的公开检出**里一键启用公开示例包。

为什么需要它：公开仓的 CMakeLists.txt 出于"公开检出不含示例资产"的考虑不追踪
`examples/` 目录；本脚本把示例包**内嵌在脚本里**（离线可用、无需网络、不依赖
GitHub 上是否已发布），并把对应的 CMake 段幂等地写进 CMakeLists.txt。

    python3 tools/enable_examples.py          # 启用（写 examples/public + 改 CMakeLists.txt）
    python3 tools/enable_examples.py --check  # 只看状态，不改任何东西
    python3 tools/enable_examples.py --reset  # 撤销（删 examples/ + 还原 CMakeLists.txt）

启用后：
    cmake -S . -B build && cmake --build build -j8 --target sim_demo_scenario
    ./build/sim_demo_scenario
"""
import argparse
import os
import shutil
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXAMPLES_DIR = os.path.join(REPO, "examples", "public")
CMAKE = os.path.join(REPO, "CMakeLists.txt")
BACKUP = os.path.join(REPO, "CMakeLists.txt.examples.bak")

CMAKE_BLOCK = """
# ── 公开示例包（examples/public，免数据库可运行）────────────────────────
# 启用方式：python3 tools/enable_examples.py（脚本会幂等地把本段写入 CMakeLists.txt）
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/public/demo_scenario.cpp)
    file(GLOB PUBLIC_EXAMPLE_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/public/*.cpp)
    list(SORT PUBLIC_EXAMPLE_SOURCES)
    foreach(example_src IN LISTS PUBLIC_EXAMPLE_SOURCES)
        get_filename_component(example_name "${example_src}" NAME_WE)
        add_executable(sim_${example_name} "${example_src}")
        target_include_directories(sim_${example_name} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/public
            ${NLOHMANN_INCLUDE_DIR}
        )
        target_link_libraries(sim_${example_name} PRIVATE sim_core)
    endforeach()
endif()
"""

# ── 内嵌示例文件（离线可用）────────────────────────────────────────────────
FILES = {
    "README.md": r'''# examples/public —— 公开示例包（免数据库、可直接运行）

> 由 `tools/enable_examples.py` 生成。目标：不装任何官方数据也能跑通一个完整闭环：
> 手工装配精灵 → 挂一个自己写的魂印 → 驱动一个回合 → 断言结果。

## 快速开始

```bash
cmake -S . -B build
cmake --build build -j8 --target sim_demo_scenario
./build/sim_demo_scenario
# 期望输出：examples/public demo PASS（断言 4 条）
```

## 目录

| 文件 | 作用 |
|---|---|
| `bare_pet.h` | 免数据库的技能/精灵装配（`skill_bare` / `pet_bare` / `party_of`） |
| `public_harness.h` | 最小对局驱动（`Battle` 类 + `check_eq` / `check_true` 断言） |
| `demo_mark.h` | 教学用示例魂印（非官方机制）：登场时给自己加 10 点护盾 |
| `demo_scenario.cpp` | 最小闭环（护盾生效 + 护盾打空 + 无印对照） |

## 怎么写你自己的第一个效果

1. 写效果函数（签名固定）：

```cpp
EffectResult my_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    // a.owner = 效果归属方（0/1）；a.opp = 对手；a.host_slot = 宿主槽
    // a.row = 官方行参（原序；缺失用 a.row_or(i, fallback)）
    return EffectResult::kOk;
}
```

2. 声明节点（时点 + 函数 + once + early + 作用域）：

```cpp
SoulMarkManager::getInstance().registerSoulMarkProgram(
    90002,
    {SOUL_NODE(State::BATTLE_ROUND_START, &my_effect,
               /*once=*/false, /*early=*/false, SoulScope::STAGE)});
```

- `once=true`：本回合只跑一次（下回合重注册复位）；
- `early=true`：**登场激活时立即执行一次**，且**同时**在该时点的桶里再执行一次
  → 同一个效果加两遍。`early` 只用于"必须在选择技能之前就位"的信号类效果
  （封技/必中旗标）。**"登场时做一次"用 `once=true` + `early=false`**；
- `SoulScope::STAGE` = 只在宿主上场时生效；`ROSTER` = 宿主存活即生效（跨切换保留）。

3. 挂到精灵上：`SoulMark(你的 id, "名字", "描述")` 塞进 `ElfPet` 的 `soul_mark` 位。

4. 断言：用 `public_harness.h` 的 `check_eq` / `check_true`（失败即非零退出，可进 CI）。

## 内核 API 去哪找

- 效果与插件契约：`include/plugin/`（`plugin_interface.h` / `soul_node_args.h` / `core_api.h`）
- 原语（伤害/回血/护盾/异常/免疫…）：`include/primitives/battle_primitives.h`
- 事件面（可监听的事件清单）：`include/effects/event_center.h`
- 机制文档：`docs/02-效果系统/`、`docs/07-工具与测试/`
- 规则票（免疫/封技/穿透/无视…）：`include/effects/rule_center.h`

## 说明

- 本包**不含**任何官方魂印/技能实现，也不含内部机制文档与场景语料——
  官方效果实现层的开放路线见仓库 README「开放路线」。
- 示例魂印 id 用 9xxxx 段（官方 effect_id 空间在 1~3000 段），避免撞号。
- 官方效果文本可从公开数据库 dump（`tools/soul_effects_dump.py`），按文本自行实现即可。
''',

    "bare_pet.h": r'''// examples/public/bare_pet.h —— 免数据库的技能/精灵装配
//
// 引擎的 `Skills(int id)` 构造会去官方库拉技能行（名字/威力/命中/PP/暴击率）——
// 公开检出没有库，所以这里**手填**：id 仅作标识，power/accuracy/pp 全部由调用方给。
//
// ⚠️ effectBranches 为空 = 该技能**不挂任何效果**（纯直伤/无附加词条的技能就是这样）。
// 需要带效果的技能：把 Effect 挂到 `effectBranches[SkillExecResult::HIT]`。
#ifndef EXAMPLES_PUBLIC_BARE_PET_H
#define EXAMPLES_PUBLIC_BARE_PET_H

#include <array>
#include <string>
#include <vector>

#include <entities/elf-pet.h>
#include <entities/mark.h>
#include <entities/skills.h>
#include <fsm/state.h>

namespace examples {

inline Skills skill_bare(int id, const std::string& name, int power, int accuracy = 100,
                         int max_pp = 5, bool must_hit = false) {
    Skills s(Skills::EmptyPetTag{});   // 空位构造：不查库、有默认字段（不抛异常）
    s.id = id;
    s.name = name;
    s.type = SkillType::Physical;
    s.power = power;
    s.accuracy = accuracy;
    s.must_hit = must_hit;
    s.priority = 0;
    s.maxPP = max_pp;
    s.pp = max_pp;
    s.critical_strike_rate = 0.0f;   // 确定性优先：暴击默认关（要验暴击自己开）
    s.element[0] = 0;
    s.element[1] = 0;
    return s;
}

inline ElfPet pet_bare(int id, const std::string& name, const std::array<Skills, 5>& skills,
                        int atk, int hp, int speed, SoulMark soul_mark = {}) {
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    // 元素 0：克制矩阵 [0][0]=1（正常 1x）；元素 1 互克会打出 0 伤害。
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, std::move(soul_mark),
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, skills);
}

// 一支 6 格队伍（后 5 格用空位占位）。
// ⚠️ ElfPet 的**默认构造被 delete**（每个成员都要显式给值），所以不能写
//    `std::array<ElfPet,6> party{}`——逐槽构造后赋值。
inline std::array<ElfPet, 6> party_of(const ElfPet& pet) {
    const std::array<Skills, 5> empty_skills{
        Skills(Skills::EmptyPetTag{}), Skills(Skills::EmptyPetTag{}),
        Skills(Skills::EmptyPetTag{}), Skills(Skills::EmptyPetTag{}),
        Skills(Skills::EmptyPetTag{})};
    std::array<ElfPet, 6> party{
        pet,
        ElfPet(1, "空位", {0, 0}, 0, Gender::NONE, SoulMark{}, CommonTrait{},
               numerical_properties{}, 0, 0, 0, false, empty_skills),
        ElfPet(2, "空位", {0, 0}, 0, Gender::NONE, SoulMark{}, CommonTrait{},
               numerical_properties{}, 0, 0, 0, false, empty_skills),
        ElfPet(3, "空位", {0, 0}, 0, Gender::NONE, SoulMark{}, CommonTrait{},
               numerical_properties{}, 0, 0, 0, false, empty_skills),
        ElfPet(4, "空位", {0, 0}, 0, Gender::NONE, SoulMark{}, CommonTrait{},
               numerical_properties{}, 0, 0, 0, false, empty_skills),
        ElfPet(5, "空位", {0, 0}, 0, Gender::NONE, SoulMark{}, CommonTrait{},
               numerical_properties{}, 0, 0, 0, false, empty_skills),
    };
    return party;
}

}  // namespace examples

#endif  // EXAMPLES_PUBLIC_BARE_PET_H
''',

    "public_harness.h": r'''// examples/public/public_harness.h —— 公开示例用的最小对局驱动
//
// 引擎的 SeerRobot / BattleContext / BattleFsm 本身就是公开内核 API，这里只做
// 三件事：①断言计数与失败退出；②两只精灵的最小对局；③操作提交。
#ifndef EXAMPLES_PUBLIC_PUBLIC_HARNESS_H
#define EXAMPLES_PUBLIC_PUBLIC_HARNESS_H

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <entities/elf-pet.h>
#include <entities/seer-robot.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/iControlBlock.h>

namespace examples {

inline int& check_count() {
    static int n = 0;
    return n;
}

inline void check_eq(long long actual, long long expected, const std::string& what) {
    ++check_count();
    if (actual != expected) {
        std::cerr << "[CHECK FAILED] " << what << "：实际 " << actual << "，期望 " << expected
                  << std::endl;
        std::exit(1);
    }
}

inline void check_true(bool ok, const std::string& what) {
    ++check_count();
    if (!ok) {
        std::cerr << "[CHECK FAILED] " << what << std::endl;
        std::exit(1);
    }
}

class Battle {
public:
    Battle(const std::array<ElfPet, 6>& party0, const std::array<ElfPet, 6>& party1)
        : robots_{SeerRobot(party0, MedicineStock{}), SeerRobot(party1, MedicineStock{})} {
        ctx_ = std::make_unique<BattleContext>(&cb_, robots_.data(), /*pvp_battle=*/false);
        fsm_ = std::make_unique<BattleFsm>(/*enable_debug=*/false);
    }

    void start() { fsm_->run(ctx_.get()); }

    // 提交一次操作：action = BattleFsm::ActionType::NONE（空过）或 SELECT_SKILL（用第 index 招）
    void submit(int player, BattleFsm::ActionType action, int index) {
        const int buf[4] = {player, static_cast<int>(action), index, 0};
        ctx_->m_buffer.assign(reinterpret_cast<const char*>(buf),
                              reinterpret_cast<const char*>(buf) + 4 * sizeof(int));
        ctx_->is_empty = false;
        fsm_->run(ctx_.get());
    }

    // 双方都空过，驱动一整个回合
    void idle_round() {
        submit(0, BattleFsm::ActionType::NONE, -1);
        submit(1, BattleFsm::ActionType::NONE, -1);
    }

    BattleContext& ctx() { return *ctx_; }

private:
    struct NoopControl : IControlBlock {
        void wait_for_input(BattleContext*) override {}
        void async_write(int, const std::string&, BattleContext*, BattleFsm*) override {}
        void on_fsm_paused(BattleContext*) override {}
    } cb_;

    std::array<SeerRobot, 2> robots_;
    std::unique_ptr<BattleContext> ctx_;
    std::unique_ptr<BattleFsm> fsm_;
};

}  // namespace examples

#endif  // EXAMPLES_PUBLIC_PUBLIC_HARNESS_H
''',

    "demo_mark.h": r'''// examples/public/demo_mark.h —— 教学用示例魂印（**非官方机制**，可直接链接）
//
// 效果：登场时给自己加 10 点护盾——护盾是内核的一等资源（ShieldBank）。
// 它演示**效果程序的形状**：签名固定、节点声明、公开内核 API 调用、公开注册面。
//
// ⚠️ **early 标志别乱用**：early 节点会在「登场激活」时立即执行一次、同时又被
//    注册进该时点的桶再执行一次 → 同登场加两次效果。`early` 只用于"必须在选择
//    技能之前就位"的信号类效果；"登场时做一次"用 once=true + early=false。
#ifndef EXAMPLES_PUBLIC_DEMO_MARK_H
#define EXAMPLES_PUBLIC_DEMO_MARK_H

#include <plugin/soul_node_args.h>

#include <effects/effect.h>
#include <entities/shield_bank.h>
#include <entities/soul_mark_manager.h>
#include <fsm/battleContext.h>

namespace examples {

// 示例魂印 id：官方 effect_id 空间在 1~3000 段，示例用 9xxxx 段避免撞号。
constexpr int kDemoMarkId = 90001;
constexpr int kDemoShield = 10;

inline EffectResult demo_mark_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    // 直接操作承伤方的护盾资源（公开 API：ElfPet::shield_bank_）。
    // 真实插件更推荐走 CoreApi 槽（grant_shield）——它带护盾优先级与来源 id。
    ctx->getPet(a.owner).shield_bank_.add(
        ShieldBank::kSoulShieldPriority, kDemoShield,
        /*source_id=*/kDemoMarkId, /*duration=*/0);
    return EffectResult::kOk;
}

// 注册一次即可（重复注册幂等：同 id 覆盖同内容）。
// 节点的效果函数走 SOUL_NODE 宏的模板 shim——它把引擎的 EffectArgs
// （绑定后 args[0..2]=owner/opp/host_slot）适配成 SoulNodeArgs。
inline void register_demo_mark() {
    SoulMarkManager::getInstance().registerSoulMarkProgram(
        kDemoMarkId,
        {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &demo_mark_enter,
                   /*once=*/true, /*early=*/false, SoulScope::STAGE)});
}

}  // namespace examples

#endif  // EXAMPLES_PUBLIC_DEMO_MARK_H
''',

    "demo_scenario.cpp": r'''// examples/public/demo_scenario.cpp —— 公开示例：一个完整可跑的最小闭环
//
// 场景：自制魂印「登场时给自己加 10 点护盾」（见 demo_mark.h）
//   R1：持有示例魂印的精灵登场 → 护盾 10 落地；
//   R2：对手打一刀（111 伤害）→ 护盾吸 10 → 剩 101 落地 → 护盾打空；
//   R3：无魂印对照 → 同样一刀 → 体力全掉。
//
// 整个场景**不查官方数据库**（技能/面板手工装配、魂印程序手动注册）。
#include <iostream>

#include "bare_pet.h"
#include "demo_mark.h"
#include "public_harness.h"

using namespace examples;

namespace {

ElfPet pet_with_demo_mark(int id, const std::string& name) {
    std::array<Skills, 5> skills{
        skill_bare(10001, "撞击", /*power=*/100),
        skill_bare(10001, "撞击", 100),
        skill_bare(10001, "撞击", 100),
        skill_bare(10001, "撞击", 100),
        skill_bare(10001, "撞击", 100),
    };
    SoulMark mark(kDemoMarkId, "示例魂印", "演示：登场获得 10 点护盾");
    return pet_bare(id, name, skills, /*atk=*/100, /*hp=*/1000, /*speed=*/200, std::move(mark));
}

ElfPet plain_pet(int id, const std::string& name) {
    std::array<Skills, 5> skills{
        skill_bare(10001, "撞击", 100), skill_bare(10001, "撞击", 100),
        skill_bare(10001, "撞击", 100), skill_bare(10001, "撞击", 100),
        skill_bare(10001, "撞击", 100),
    };
    return pet_bare(id, name, skills, /*atk=*/100, /*hp=*/1000, /*speed=*/100);
}

}  // namespace

int main() {
    // 注册示例魂印程序（公开 API，不依赖插件动态库/数据库）。
    register_demo_mark();

    // 第一段：带示例魂印的精灵，护盾吸收对手一击
    {
        Battle b(party_of(pet_with_demo_mark(1, "护盾方")),
                 party_of(plain_pet(2, "攻击方")));
        b.start();
        // R1：登场时点护盾已落地。
        check_eq(b.ctx().getPet(0).shield_bank_.total(), 10,
                 "示例魂印：登场获得 10 点护盾");
        // R2：对手出撞击（伤害 111）→ 护盾吸 10 → 剩 101 落地。
        const int hp_before = b.ctx().getPet(0).hp;
        b.submit(0, BattleFsm::ActionType::NONE, -1);
        b.submit(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        check_eq(b.ctx().getPet(0).hp, hp_before - 101,
                 "示例魂印：护盾吸收 10 点，其余 101 点落地");
        check_eq(b.ctx().getPet(0).shield_bank_.total(), 0,
                 "示例魂印：护盾被这一刀打空");
    }

    // 第二段：无魂印对照，同样一刀 → 体力真的掉
    {
        Battle b(party_of(plain_pet(1, "无印方")), party_of(plain_pet(2, "攻击方")));
        b.start();
        const int hp_before = b.ctx().getPet(0).hp;
        b.submit(0, BattleFsm::ActionType::NONE, -1);
        b.submit(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        check_true(b.ctx().getPet(0).hp < hp_before, "对照：无魂印时体力确实下降");
    }

    std::cout << "examples/public demo PASS（断言 " << check_count() << " 条）" << std::endl;
    return 0;
}
''',
}


def already_enabled() -> bool:
    if not os.path.exists(CMAKE):
        return False
    with open(CMAKE, encoding="utf-8") as f:
        return "PUBLIC_EXAMPLE_SOURCES" in f.read()


def write_examples() -> None:
    os.makedirs(EXAMPLES_DIR, exist_ok=True)
    for name, content in FILES.items():
        path = os.path.join(EXAMPLES_DIR, name)
        with open(path, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"  + {os.path.relpath(path, REPO)}")


def patch_cmake() -> None:
    if already_enabled():
        print("  = CMakeLists.txt 已含示例段（幂等，跳过）")
        return
    with open(CMAKE, encoding="utf-8") as f:
        text = f.read()
    if not os.path.exists(BACKUP):
        with open(BACKUP, "w", encoding="utf-8") as f:
            f.write(text)
        print(f"  + 已备份原 CMakeLists.txt → {os.path.relpath(BACKUP, REPO)}")
    anchor = "if(BUILD_TRAINING_CLI)"
    if anchor in text:
        pos = text.index(anchor)
    else:
        pos = len(text)
    text = text[:pos] + CMAKE_BLOCK + "\n" + text[pos:]
    with open(CMAKE, "w", encoding="utf-8") as f:
        f.write(text)
    print("  + CMakeLists.txt 已插入示例段")


def reset() -> None:
    if os.path.isdir(os.path.join(REPO, "examples")):
        shutil.rmtree(os.path.join(REPO, "examples"))
        print("  - 已删除 examples/")
    if os.path.exists(BACKUP):
        shutil.copy2(BACKUP, CMAKE)
        os.remove(BACKUP)
        print("  - 已还原 CMakeLists.txt")
    print("复位完成（残留未跟踪改动请自行 git checkout）")


def main() -> int:
    ap = argparse.ArgumentParser(description="在公开检出里启用 examples/public 示例包")
    ap.add_argument("--check", action="store_true", help="只看状态，不改文件")
    ap.add_argument("--reset", action="store_true", help="撤销启用")
    args = ap.parse_args()

    print(f"仓库根：{REPO}")
    if args.reset:
        reset()
        return 0

    have_dir = os.path.exists(os.path.join(EXAMPLES_DIR, "demo_scenario.cpp"))
    print(f"状态：CMake 段={'已启用' if already_enabled() else '未启用'}；"
          f"示例目录={'已存在' if have_dir else '缺失'}")
    if args.check:
        return 0

    if not have_dir:
        print("写入示例文件（内嵌，离线可用）：")
        write_examples()
    else:
        print("示例目录已存在，跳过写入")
    patch_cmake()

    print("\n下一步：")
    print("  cmake -S . -B build")
    print("  cmake --build build -j8 --target sim_demo_scenario")
    print("  ./build/sim_demo_scenario")
    return 0


if __name__ == "__main__":
    sys.exit(main())
