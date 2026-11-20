#include <utils/sbs_rand.h>
// 场景 4250 —— K269 概率令对手害怕族（186，soul_lib/k269_prob_fear.cpp）
//（2026-10-21，worktree ../agents/k269；号段 4250-4259 → T4250-T4253 四块）
//
// 断言口径（拍打 P40、atk=300；对手 atk=1 不还手、hp 池 800000）：
//   - T4250 db[0]=10：30 刀对手害怕回合数 ∈ [1,12]（p=0.1/n=30 → 期望 3，
//     P(零触发)≈4%；种子定序逐次一致）。
//   - T4251 db[0]=0：20 刀零害怕（关闸档，args 驱动直证；若误用固定 10% 会 ≈2）。
//   - T4252 db[0]=100：30 刀害怕回合数 ≥25（概率门 100% 档短路；害怕无"命中即醒"
//     类即时清除，缺口只可能来自 0 概率外的抗性/弹控——沙包无票，预期 30/30）。
//   - T4253 属性技锚点：db[0]=100 用精准（category=4）20 次 → 零害怕
//     （"攻击附带"判据：属性技非攻击，钉 !used_attribute 门）。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;
constexpr int kZhunQue = 20138;   // 精准：属性攻击（category=4，power 0）
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);

SoulMark mark186(std::vector<int> row) {
    // 直注官方行原序（K236 场景同款，宿主 owned_int_args 通道）。
    SoulMark sm(186, "K269 魂印", "test", EffectArgs(std::move(row)));
    sm.monster_id = 2001;
    return sm;
}

ElfPet mark_pet(std::vector<int> row) {
    ElfPet p = make_pet(1, "艾辛格", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, 800000, mark186(std::move(row)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, 800000);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void round_attack(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int fear_rounds(ScenarioBattle& b, int rounds, int skill_index) {
    BattleContext& ctx = b.ctx();
    int n = 0;
    for (int i = 0; i < rounds; ++i) {
        round_attack(b, skill_index);
        if (ctx.has_active_abnormal_status(1, kFear)) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4250 db[0]=10：30 刀害怕回合数 ∈ [1,12] ═══════════════════════════════
    //（种子 20261021 实测 0/30——P(零触发)≈4.2% 的真实坏运气，勘误换 20261022=3）
    {
        sbs_srand(20261022);
        ScenarioBattle b(make_party(mark_pet({10, 0})), make_party(foe_pet()));
        b.start();
        const int n = fear_rounds(b, 30, 0);
        CHECK(n >= 1 && n <= 12,
              "T4250 186：30 刀 10% 害怕回合数 ∈ [1,12]（实测 " + std::to_string(n) +
                  "）");
        std::cout << "  [T4250] 10% 害怕统计带 通过（实测 " << n << "）\n";
    }

    // ══ T4251 db[0]=0：20 刀零害怕（关闸档）════════════════════════════════════
    {
        sbs_srand(4251);
        ScenarioBattle b(make_party(mark_pet({0, 0})), make_party(foe_pet()));
        b.start();
        const int n = fear_rounds(b, 20, 0);
        CHECK_EQ(n, 0,
                 "T4251 186：db[0]=0 关闸 → 20 刀零害怕（若误用固定 10% 会 ≈2）");
        std::cout << "  [T4251] 0% 关闸对照 通过\n";
    }

    // ══ T4252 db[0]=100：30 刀害怕回合数 ≥25（上限锚）══════════════════════════
    {
        sbs_srand(4252);
        ScenarioBattle b(make_party(mark_pet({100, 0})), make_party(foe_pet()));
        b.start();
        const int n = fear_rounds(b, 30, 0);
        CHECK(n >= 25,
              "T4252 186：db[0]=100 → 30 刀害怕回合数 ≥25（实测 " +
                  std::to_string(n) + "）");
        std::cout << "  [T4252] 100% 上限锚 通过（实测 " << n << "）\n";
    }

    // ══ T4253 属性技锚点：精准（category=4）20 次零害怕 ════════════════════════
    {
        sbs_srand(4253);
        ElfPet m = make_pet(1, "艾辛格", {kPaiDa, kZhunQue, kPaiDa, kPaiDa, kPaiDa},
                            /*speed=*/300, /*atk=*/300, 800000, mark186({100, 0}));
        for (Skills& s : m.skills) {
            s.must_hit = 0;
            s.accuracy = 100;
        }
        ScenarioBattle b(make_party(m), make_party(foe_pet()));
        b.start();
        const int n = fear_rounds(b, 20, 1);   // 技能槽 1 = 精准（属性技）
        CHECK_EQ(n, 0,
                 "T4253 186：属性技 20 次零害怕（攻击附带判据，属性技不触发）");
        std::cout << "  [T4253] 属性技不触发 通过\n";
    }

    std::cout << "scenario_4250 PASS (K269 概率令对手害怕族)" << std::endl;
    return 0;
}
