#include <utils/sbs_rand.h>
// 场景 7230 —— K377 体力对照双分支暴击/闪避族（1354，soul_lib/k377_hpcrit_dodge_branch.cpp）
//（2026-10-24，worktree ../agents/k377；号段 7230-7239 → T7230.a-g 七块）
//
// 断言口径（1354 高体→攻技 P% 暴击未触发 100% 害怕；低体→P% 闪避未触发下回合
// 攻击伤害 +50%）：max=100000；高体=holder 90000 / foe 1000；低体反之。
//   快照：step_past(ROUND_START) 读 ws.crit_rate_add / ws.dodge_rate。
//   行为：暴击 = crit_happened[0]；害怕 = has_active_abnormal_status(1,6)；
//         闪避 = foe 攻击后 holder 掉血与否（种子探针）；
//         补偿 = 下回合 holder 攻击伤害带 [128,158]（[87,102]×1.5±2）。
#include <cstdlib>
#include <iostream>
#include <utility>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;
constexpr int kMaxHp = 100000;

SoulMark make_mark(std::vector<int> args) {
    SoulMark sm(1354, "K377 尤尼卡", "test", EffectArgs(std::move(args)));
    sm.monster_id = 3777;
    return sm;
}

ElfPet mark_pet(std::vector<int> args) {
    ElfPet p = make_pet(1, "尤尼卡", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark(std::move(args)));
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        // ⚠️ must_hit=1 会跳过 dodge_rate 减避（battleWorkspace.h:291 必闪词条无效
        // 同源）——闪避支测试对手必须走普通命中链
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void holder_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int foe_attack(ScenarioBattle& b) {
    const int hp0 = b.ctx().getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp0 - b.ctx().getPet(0).hp;   // 掉血 0 = 被闪避
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7230.a HIGH 快照：crit_rate_add=50 ══
    {
        sbs_srand(7230);
        ScenarioBattle b(make_party(mark_pet({50, 100, 50, 50, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 90000;
        b.ctx().getPet(1).hp = 1000;
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.ctx().debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(b.ctx().ws.crit_rate_add[0] > 49.5f && b.ctx().ws.crit_rate_add[0] < 50.5f,
              "T7230.a HIGH 支 crit_add=50（实测 " +
                  std::to_string(b.ctx().ws.crit_rate_add[0]) + "）");
        std::cout << "  [T7230.a] HIGH 快照 通过\n";
    }

    // ══ T7230.b HIGH 满档暴击：db0=100 → 必暴且不害怕 ══
    {
        sbs_srand(7231);
        ScenarioBattle b(make_party(mark_pet({100, 100, 50, 50, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 90000;
        b.ctx().getPet(1).hp = 1000;
        holder_attack(b);
        CHECK_EQ(b.ctx().crit_happened[0], true, "T7230.b 高体 100 点必暴");
        CHECK(!b.ctx().has_active_abnormal_status(1, 6),
              "T7230.b 暴击触发 → 不害怕（互斥）");
        std::cout << "  [T7230.b] 满档必暴 通过\n";
    }

    // ══ T7230.c HIGH 未触发 → 害怕（探针 db0=20 扫暴/不暴两支）══
    {
        int no_crit = -1, crit = -1;
        for (int s = 1; s < 400 && (no_crit < 0 || crit < 0); ++s) {
            sbs_srand(s);
            ScenarioBattle b(make_party(mark_pet({20, 100, 50, 50, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 90000;
            b.ctx().getPet(1).hp = 1000;
            holder_attack(b);
            if (b.ctx().crit_happened[0]) {
                if (crit < 0 && !b.ctx().has_active_abnormal_status(1, 6)) {
                    crit = s;
                }
            } else if (no_crit < 0 && b.ctx().has_active_abnormal_status(1, 6)) {
                no_crit = s;
            }
        }
        CHECK(no_crit > 0, "T7230.c 探针：未暴击 → 害怕（seed " +
                               std::to_string(no_crit) + "）");
        CHECK(crit > 0, "T7230.c 对照：暴击 → 不害怕（seed " +
                            std::to_string(crit) + "）");
        std::cout << "  [T7230.c] 未暴击害怕/对照 通过（" << no_crit << "/" << crit
                  << "）\n";
    }

    // ══ T7230.d LOW 快照：dodge_rate=0.5 ══
    {
        sbs_srand(7232);
        ScenarioBattle b(make_party(mark_pet({50, 100, 50, 50, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 1000;
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.ctx().debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(b.ctx().ws.dodge_rate[0] > 0.495f && b.ctx().ws.dodge_rate[0] < 0.505f,
              "T7230.d LOW 支 dodge_rate=0.5（实测 " +
                  std::to_string(b.ctx().ws.dodge_rate[0]) + "）");
        std::cout << "  [T7230.d] LOW 快照 通过\n";
    }

    // ══ T7230.e LOW 闪避行为：探针扫 命中/闪避两支 ══
    {
        int hit_seed = -1, dodge_seed = -1;
        for (int s = 1; s < 400 && (hit_seed < 0 || dodge_seed < 0); ++s) {
            sbs_srand(s);
            ScenarioBattle b(make_party(mark_pet({50, 100, 50, 50, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 1000;
            const int d = foe_attack(b);
            if (d > 0 && hit_seed < 0) {
                hit_seed = s;
            }
            if (d == 0 && dodge_seed < 0) {
                dodge_seed = s;
            }
        }
        CHECK(hit_seed > 0, "T7230.e 探针：闪避未触发 → 掉血（seed " +
                                std::to_string(hit_seed) + "）");
        CHECK(dodge_seed > 0, "T7230.e 探针：闪避触发 → 零伤（seed " +
                                  std::to_string(dodge_seed) + "）");
        std::cout << "  [T7230.e] 闪避两支 通过（" << hit_seed << "/" << dodge_seed
                  << "）\n";
    }

    // ══ T7230.f LOW 被击 → 下回合增伤补偿：伤害带 [128,158] ══
    {
        int hit_seed = -1;
        for (int s = 1; s < 400 && hit_seed < 0; ++s) {
            sbs_srand(s);
            ScenarioBattle b(make_party(mark_pet({50, 100, 50, 50, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 1000;
            if (foe_attack(b) > 0) {
                hit_seed = s;
            }
        }
        CHECK(hit_seed > 0, "T7230.f 前置：找到被击 seed");
        sbs_srand(hit_seed);
        ScenarioBattle b(make_party(mark_pet({50, 100, 50, 50, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 1000;
        (void)foe_attack(b);   // R1：被击 → 补偿账本
        const int hp0 = b.ctx().getPet(1).hp;
        holder_attack(b);      // R2：补偿 +50%
        const int d = hp0 - b.ctx().getPet(1).hp;
        CHECK(d >= 128 && d <= 158,
              "T7230.f 补偿增伤 [87,102]×1.5：伤害 " + std::to_string(d) +
                  " ∈ [128,158]");
        std::cout << "  [T7230.f] 下回合补偿增伤 通过（d=" << d << "）\n";
    }

    // ══ T7230.g 等号档：无分支无效果 ══
    {
        sbs_srand(7233);
        ScenarioBattle b(make_party(mark_pet({50, 100, 50, 50, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 50000;
        b.ctx().getPet(1).hp = 50000;
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.ctx().debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(b.ctx().ws.crit_rate_add[0] == 0.0f && b.ctx().ws.dodge_rate[0] == 0.0f,
              "T7230.g 等号档：无暴击/闪避加成");
        holder_attack(b);
        CHECK_EQ(b.ctx().crit_happened[0], false, "T7230.g 等号档不暴");
        CHECK(!b.ctx().has_active_abnormal_status(1, 6), "T7230.g 等号档不害怕");
        std::cout << "  [T7230.g] 等号无分支 通过\n";
    }

    std::cout << "scenario_7230 PASS (K377 体力对照双分支暴击/闪避族)" << std::endl;
    return 0;
}
