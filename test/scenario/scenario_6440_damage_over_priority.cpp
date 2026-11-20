// 场景 6440 —— J311 伤害高于门→下N回合先制族（591，
// moves_lib/j_damage_over_priority.cpp）（2026-10-22，worktree feat/jbatch；号段 6440-6449）
//
// 断言口径（审计槽 + 先手权结构断言 + 绝对量带）：
//   - 触发面 = 伤害管线 DETECT 相位读 `resolvedDamage.final`：
//     **大于{0}** = 严格大于阈值（等值不触发）；只认落地红伤（isRed && final>0）。
//   - 审计槽 991591100 = 触发那一发的 final（-999 = 未触发）；991591101 = 授予的窗口回合数。
//   - 授予面 = "下{1}回合自己所有直接攻击先制+{2}"：NextRounds 窗（MOVE_RIGHT 桶），
//     只给**攻击技**加先制（属性技不吃，481 同判据）。
//   - **先手权结构断言**（比数值更硬的观测面）：先制等级不等时先手权**只看等级不看速度**
//     （battleFsm handle_BattleFirstMoveRight：preemptive_level[0] vs [1] 不等则直接定，
//     相等才比速度）→ 己方速度 50 < 对手 300 仍能抢到先手 = 等级确实加了。
//     属性技不吃时等级相等 → 回落速度 → 对手先手（对照组）。
//   - 伤害带按官方公式反推：damage = (0.84×攻/防×威力+2)×克制1.0×(217+rand%39)/255
//     → 极光追魂灭(物150)@攻300 = 380×[0.851,1.0] → [323,380]（>260 触发）；
//     @攻100 = 128×[0.851,1.0] → [108,128]（≤260 门闭）。
#include <iostream>
#include <any>
#include <string>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;       // 拍打：物 40（攻击技填充）
constexpr int kGaoSu = 20017;       // 高速移动：属性技（无伤害；自带"自身速度+2"，不影响本组断言）
constexpr int k591Carrier = 16629;  // 极光追魂灭 [591]|[260,2,1]（纯链，物 150）

constexpr int k591Audit = 991591100;
constexpr int k591Grant = 991591101;

constexpr int kSlow = 50;    // 己方慢速（先制等级不等时仍能抢到先手 = 等级生效的硬证据）
constexpr int kFast = 300;   // 对手高速

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_owner(int atk) {
    ElfPet p = make_pet(1, "己方", {k591Carrier, kPaiDa, kGaoSu, kPaiDa, kPaiDa},
                        /*speed=*/kSlow, /*atk=*/atk, /*hp=*/4000);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kGaoSu, kPaiDa, kGaoSu, kPaiDa, kPaiDa},
                        /*speed=*/kFast, /*atk=*/1, /*hp=*/4000);
    accuracy_100(p);
    return p;
}

int audit(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -999;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -999;
}

// 己方打第 slot 个技能（对手用属性技 filler，不还手不致死）。
void play(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

bool owner_first(ScenarioBattle& b) {
    return b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6440.a 591 触发（伤害 > 260）→ 下 2 回合攻击技先制+1（低速抢到先手）══
    {
        ScenarioBattle b(make_party(mk_owner(/*atk=*/300)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        play(b, /*slot=*/0);   // 载体技：伤害 > 260 → 触发
        const int dealt = 4000 - ctx.getPet(1).hp;
        std::cout << "  [T6440.a] R1 damage=" << dealt << " audit=" << audit(ctx, k591Audit)
                  << " grant=" << audit(ctx, k591Grant)
                  << " R1_owner_first=" << owner_first(b) << std::endl;
        CHECK(dealt >= 318 && dealt <= 385,
              "T6440.a 载体伤害落在 [323,380]（>260，实测 " + std::to_string(dealt) + "）");
        CHECK_EQ(audit(ctx, k591Audit), dealt, "T6440.a 审计槽=触发那一发的 final");
        CHECK_EQ(audit(ctx, k591Grant), 2, "T6440.a 授予窗口 2 回合");
        CHECK(!owner_first(b), "T6440.a 触发回合尚未开窗 → 己方仍后手（速度劣势）");
        play(b, /*slot=*/1);   // 拍打（攻击技）→ 窗内 R2
        CHECK(owner_first(b), "T6440.a 窗内 R2 攻击技 → 先制+1 抢到先手（低速反超）");
        play(b, /*slot=*/1);   // 拍打（攻击技）→ 窗内 R3
        CHECK(owner_first(b), "T6440.a 窗内 R3 攻击技 → 先制+1 仍生效（第 2 回合）");
        play(b, /*slot=*/1);   // 拍打（攻击技）→ 窗过期 R4
        CHECK(!owner_first(b), "T6440.a 窗过期 R4 → 恢复速度对比（己方仍后手）");
    }

    // ══ T6440.b 门闭：伤害 ≤ 260 → 不触发、不开窗 ══
    {
        ScenarioBattle b(make_party(mk_owner(/*atk=*/100)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        play(b, /*slot=*/0);   // 载体技：伤害 ≤ 260 → 门闭
        const int dealt = 4000 - ctx.getPet(1).hp;
        std::cout << "  [T6440.b] R1 damage=" << dealt << " audit=" << audit(ctx, k591Audit)
                  << " grant=" << audit(ctx, k591Grant) << std::endl;
        CHECK(dealt >= 105 && dealt <= 132,
              "T6440.b 载体伤害落在 [108,128]（≤260，实测 " + std::to_string(dealt) + "）");
        CHECK_EQ(audit(ctx, k591Audit), -999, "T6440.b 门闭 → 审计未写");
        CHECK_EQ(audit(ctx, k591Grant), -999, "T6440.b 门闭 → 未授予窗口");
        play(b, /*slot=*/1);   // 拍打（攻击技）
        CHECK(!owner_first(b), "T6440.b 门闭 → R2 攻击技也抢不到先手（无窗口）");
    }

    // ══ T6440.c 窗内用**属性技** → 不吃先制（"所有直接攻击"限定）══
    {
        ScenarioBattle b(make_party(mk_owner(/*atk=*/300)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        play(b, /*slot=*/0);   // 载体技：触发
        CHECK_EQ(audit(ctx, k591Grant), 2, "T6440.c 已开窗（2 回合）");
        play(b, /*slot=*/2);   // 高速移动（属性技）
        CHECK(!owner_first(b), "T6440.c 窗内用属性技 → 不吃先制（仍后手）");
        play(b, /*slot=*/1);   // 拍打（攻击技）→ 窗内第 2 回合
        CHECK(owner_first(b), "T6440.c 窗内 R3 改用攻击技 → 先制+1 生效");
    }

    std::cout << "scenario_6440 PASS (J311 伤害高于门→下N回合先制族)" << std::endl;
    return 0;
}
