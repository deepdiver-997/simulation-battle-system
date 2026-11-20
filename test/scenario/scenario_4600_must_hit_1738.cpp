// 场景 4600 —— K304 必中族（1738，soul_lib/k304_must_hit.cpp）
//（2026-10-21，worktree ../agents/k304；号段 4600-4609 → T4600-T4602 三块）
//
// 断言口径（1738「以爱的名义，使自身所有技能必定命中！」）：
//   - T4600 **必中生效**：对手带 100% 闪避魂印（正常必 miss）+ 印主带 1738 必中
//     → 印主的攻击**仍命中**（必中 = 跳过命中率掷骰）。对照：同样配置但印主
//     **不带** 1738 → 必 miss。
//   - T4601 门闭对照：印主不带 1738（对手 100% 闪避）→ 攻击 miss。
//   - T4602 **每回合生效**（常驻非首回合限定）：连续两回合出手，两次都命中
//     ——证明 FMR 每回合重授（ws 每回合 reset → 不重授则第 2 回合失效）。
//
// ⚠️ 场景构造口径：零参行 → args 传 {0,0}（DB 原样；实现不消费任何下标）。
// ⚠️ **必中的构造**：对手带 harness 的 `make_dodge_soul_mark()`（闪避 100%）
//   → 无必中凭证时必 miss；印主带 1738 时**跳过掷骰** → 仍命中。这是"必中"
//   最干净的判据（比 accuracy=0 更可控——闪避是 100% 确定性，accuracy=0 还要
//   看引擎的命中等级档位折算）。
// ⚠️ 判定"是否命中"读**对手掉血**（>0 即命中）；不是读 hit 事件。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;    // 撞击：攻击载体
constexpr int kMaxHp = 99999;

SoulMark make_mark() {
    // 零参行：DB args 原样 {0,0}，实现不消费任何下标。
    SoulMark sm(1738, "K304 魂印", "test", EffectArgs(std::vector<int>{0, 0}));
    sm.monster_id = 4150;
    return sm;
}

ElfPet mark_pet(bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "拂晓兔", {kJi, kJi, kJi, kJi, kJi},
                   /*speed=*/300, /*atk=*/100, kMaxHp, make_mark())
        : make_pet(1, "裸拂晓兔", {kJi, kJi, kJi, kJi, kJi},
                   /*speed=*/300, /*atk=*/100, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;      // 技能自身**不**必中（必中只能来自本魂印）
        s.accuracy = 100;
    }
    return p;
}

// 对手带 100% 闪避（harness 造法）。
ElfPet dodge_pet() {
    ElfPet p = make_pet(2, "闪避对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/10, kMaxHp,
                        /*soul_mark=*/make_dodge_soul_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 普通对手（无闪避）。
ElfPet plain_pet() {
    ElfPet p = make_pet(2, "普通对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/10, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 印主出一技；返回对手掉血（0 = miss）。
int mark_attacks(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return before - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4600 必中生效：对手 100% 闪避 + 印主带 1738 → 仍命中 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/true)),
                         make_party(dodge_pet()));
        b.start();
        const int loss = mark_attacks(b);
        CHECK(loss > 0,
              "T4600 1738：对手 100% 闪避，印主带必中魂印 → 仍命中（对手掉血 "
                  + std::to_string(loss) + "）");
        std::cout << "  [T4600] 必中生效 通过 (loss=" << loss << ")\n";
    }

    // ══ T4601 门闭对照：印主不带 1738 → 必 miss ══
    {
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/false)),
                         make_party(dodge_pet()));
        b.start();
        const int loss = mark_attacks(b);
        CHECK_EQ(loss, 0,
                 "T4601 1738：对照——不带魂印时对手 100% 闪避 → 必 miss（对手掉血 0）");
        std::cout << "  [T4601] 门闭对照 通过\n";
    }

    // ══ T4602b **弱化下仍必中**（区分两条通道的关键判据）══
    // ⚠️ 这是**区分必中凭证与"命中率钉100"通道**的唯一判据：
    //   · ws.must_hit_grant（真必中）= **跳过命中率掷骰** → 命中等级负档也照样命中；
    //   · ws.accuracy_fix_100（"基础命中率钉100"）= 只把 roll 起点钉 100，**命中
    //     等级档位仍然乘算** → 弱化（命中等级 -1）后**仍可能 Miss**
    //     （battleWorkspace.h:288-293 明写：它**≠ 必中凭证**）。
    //   故：印主命中等级 -1 + 印主带 1738 → 仍命中（真必中）；
    //       同配置但用 accuracy_fix 通道 → 会 miss（实测变异 MUT1 靠此判据转红）。
    {
        // 命中等级 -6 → 命中率折算 25%（hit_level_accuracy_pct，-6 档）→
        // 若是"钉 100 起点"通道（accuracy_fix_100），仍有 75% 概率 miss；
        // 若是真必中（跳过掷骰）→ **必命中**。故采 10 局：真必中 = 10/10 命中。
        int hits = 0;
        const int rounds = 10;
        for (int i = 0; i < rounds; ++i) {
            ScenarioBattle b(make_party(mark_pet(/*with_mark=*/true)),
                             make_party(plain_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.ability_levels[0][5] = -6;   // 命中等级 -6（命中率 25%）
            if (mark_attacks(b) > 0) {
                ++hits;
            }
        }
        CHECK_EQ(hits, rounds,
                 "T4602b 1738：命中等级弱化到 -6（命中率 25%）→ 仍**必**命中（必中=跳过"
                 "掷骰）；实测命中 " + std::to_string(hits) + "/" + std::to_string(rounds));
        std::cout << "  [T4602b] 弱化下仍必中 通过 (" << hits << "/" << rounds << ")\n";
    }

    // ══ T4602 每回合生效：连续两回合都命中 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/true)),
                         make_party(dodge_pet()));
        b.start();
        const int l1 = mark_attacks(b);
        const int l2 = mark_attacks(b);
        CHECK(l1 > 0, "T4602 第 1 回合命中（实测 " + std::to_string(l1) + "）");
        CHECK(l2 > 0,
              "T4602 1738：第 2 回合仍命中（常驻必中，FMR 每回合重授；ws 每回合 "
              "reset → 不重授则第 2 回合失效），实测 " + std::to_string(l2));
        std::cout << "  [T4602] 每回合生效 通过\n";
    }

    std::cout << "scenario_4600 PASS (K304 必中族)" << std::endl;
    return 0;
}
