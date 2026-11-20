#include <utils/sbs_rand.h>
// 场景 7220 —— K375 异常门随机降属+护盾仅1次族（385，soul_lib/k375_anomaly_gate_shield_once.cpp）
//（2026-10-24，worktree ../agents/k375；号段 7220-7229 → T7220-T7226 七块）
//
// 断言口径（白盒槽读 + 原语面精确断言，K369/K372 同风格）：
//   - 降属面：ability_levels[对手][*]（R1 恰 2 个 -1 去重槽；R2 叠放 sum -4）。
//   - 护盾面：shield_bank_.total() 精确读（授予 300；仅1次=once 旗，破盾不复活）。
//   - 异常注入：apply_abnormal_status_for_rounds（回合间注入，分支时点确定）。
//   - 面板面：PRE_BATTLE 后 numericalProperties 精确读（row458 size>=7 护栏）。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/shield_bank.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10004;     // 拍打（物理攻击）
constexpr int kMaxHp = 800000;

SoulMark make_mark(const std::vector<int>& args) {
    // DB args 原序：db0=?(3) db1=项数 db2=降幅 db3=?(3) db4=护盾量 [db5=攻+ db6=体+]。
    SoulMark sm(385, "K375 魂印", "test", EffectArgs(args));
    sm.monster_id = 2780;
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int atk = 100) {
    ElfPet p = make_pet(1, "战神盖亚", {kHit, kHit, kHit, kHit, kHit},
                        /*speed=*/100, /*atk=*/atk, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int atk = 1) {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit},
                        /*speed=*/300, /*atk=*/atk, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void use_round(ScenarioBattle& b, int slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// 对手出手回合（对手速度快先手；印主 NONE——烧伤不打自己伤害面）。
void foe_attack_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

int foe_level_sum(ScenarioBattle& b) {
    int sum = 0;
    for (int i = 0; i < 6; ++i) {
        sum += b.ctx().ability_levels[1][i];
    }
    return sum;
}

int foe_negative_slots(ScenarioBattle& b) {
    int n = 0;
    for (int i = 0; i < 6; ++i) {
        if (b.ctx().ability_levels[1][i] < 0) {
            ++n;
        }
    }
    return n;
}

int holder_shield(ScenarioBattle& b) {
    return b.ctx().getPet(0).shield_bank_.total();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7220 无异常门：每回合随机去重 2 项 -1 ════════════════════════════════
    {
        sbs_srand(7220);
        ScenarioBattle b(make_party(mark_pet({3, 2, 1, 3, 300})), make_party(foe_pet()));
        b.start();
        use_round(b);
        CHECK_EQ(foe_negative_slots(b), 2, "T7220 R1：恰 2 个 -1 槽（去重）");
        CHECK_EQ(foe_level_sum(b), -2, "T7220 R1：sum -2");
        use_round(b);
        CHECK_EQ(foe_level_sum(b), -4, "T7220 R2：叠放 sum -4");
        CHECK_EQ(holder_shield(b), 0, "T7220 无异常全程无护盾");
        std::cout << "  [T7220] 无异常随机降属 通过\n";
    }

    // ══ T7221 异常门→护盾授予（仅1次：异常持续也不叠加）═══════════════════════
    {
        sbs_srand(7221);
        ScenarioBattle b(make_party(mark_pet({3, 2, 1, 3, 300})), make_party(foe_pet()));
        b.start();
        use_round(b);   // R1：无异常 → 降属支
        b.ctx().apply_abnormal_status_for_rounds(0, static_cast<int>(AbnormalStatusId::Burn),
                                                 9);
        use_round(b);   // R2：异常门 → 授盾 300（无降属）
        CHECK_EQ(holder_shield(b), 300, "T7221 核心：异常→300 护盾");
        const int sum_after_r2 = foe_level_sum(b);
        use_round(b);   // R3：仍异常 → once 旗已置 → 不叠加、不降属
        CHECK_EQ(holder_shield(b), 300, "T7221 仅1次：不叠加");
        CHECK_EQ(foe_level_sum(b), sum_after_r2, "T7221 异常期不降属");
        std::cout << "  [T7221] 异常门授盾+仅1次 通过\n";
    }

    // ══ T7222 破盾后不复活（once 旗跨破盾持续）════════════════════════════════
    {
        sbs_srand(7222);
        ScenarioBattle b(make_party(mark_pet({3, 2, 1, 3, 300})),
                         make_party(foe_pet(/*atk=*/1200)));
        b.start();
        use_round(b);   // R1：降属支
        // 害怕(6)：控制类无伤害 tick——烧伤/中毒会 tick 污染 hp_loss 断言。
        b.ctx().apply_abnormal_status_for_rounds(0, static_cast<int>(AbnormalStatusId::Fear),
                                                 9);
        const int hp0 = b.ctx().getPet(0).hp;
        foe_attack_round(b);   // R2：对手先手攻（raw [345,408]）→ 盾吸收 300 破盾
        CHECK_EQ(holder_shield(b), 0, "T7222 盾被击破（吸收 300 后归零）");
        const int hp_loss = hp0 - b.ctx().getPet(0).hp;
        CHECK(hp_loss >= 40 && hp_loss <= 115,
                   "T7222 穿透 = raw-300 ∈ [45,108]（外扩余量）");
        foe_attack_round(b);   // R3：仍异常但 once 已置 → 无新盾，伤害直落
        CHECK_EQ(holder_shield(b), 0, "T7222 核心：破盾后不复活（仅1次）");
        std::cout << "  [T7222] 破盾不复活 通过\n";
    }

    // ══ T7223 门翻回：异常到期 → 降属支恢复、盾不复授 ═════════════════════════
    {
        sbs_srand(7223);
        ScenarioBattle b(make_party(mark_pet({3, 2, 1, 3, 300})), make_party(foe_pet()));
        b.start();
        use_round(b);   // R1：降属支
        // duration=1：回合间注入 end_round=roundCount+1 → 仅 R2 生效、R3 前到期。
        b.ctx().apply_abnormal_status_for_rounds(0, static_cast<int>(AbnormalStatusId::Burn),
                                                 1);
        use_round(b);   // R2：异常门 → 授盾
        CHECK_EQ(holder_shield(b), 300, "T7223 R2 异常期授盾");
        const int sum_r2 = foe_level_sum(b);
        use_round(b);   // R3：烧伤(2回合)已到期 → 门翻回降属支；once 已置不重授
        CHECK(!b.ctx().has_any_active_abnormal_status(0),
                   "T7223 前置：异常已到期");
        CHECK_EQ(foe_level_sum(b), sum_r2 - 2, "T7223 核心：门翻回降属恢复");
        CHECK_EQ(holder_shield(b), 300, "T7223 盾保持 300（不重授不消失）");
        std::cout << "  [T7223] 门翻回降属 通过\n";
    }

    // ══ T7224 伙伴行面板（row458 size>=7）：攻+50 体+50，主句照常 ══════════════
    {
        sbs_srand(7224);
        ScenarioBattle b(make_party(mark_pet({3, 2, 1, 3, 300, 50, 50})),
                         make_party(foe_pet()));
        // PRE_BATTLE 在 BattleContext 构造期已落笔。
        ElfPet& me = b.ctx().getPet(0);
        CHECK_EQ(me.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK], 150,
                 "T7224 伙伴行：攻击面板 +50");
        CHECK_EQ(me.numericalProperties[NumericalPropertyIndex::HP], kMaxHp + 50,
                 "T7224 伙伴行：体力面板 +50");
        b.start();
        use_round(b);
        CHECK_EQ(foe_negative_slots(b), 2, "T7224 主句照常：降属支");
        std::cout << "  [T7224] 伙伴行面板+主句 通过\n";
    }

    // ══ T7225 本体行（row160 size=5）：面板不落笔（护栏）═══════════════════════
    {
        sbs_srand(7225);
        ScenarioBattle b(make_party(mark_pet({3, 2, 1, 3, 300})), make_party(foe_pet()));
        ElfPet& me = b.ctx().getPet(0);
        CHECK_EQ(me.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK], 100,
                 "T7225 本体行：攻击面板不动");
        CHECK_EQ(me.numericalProperties[NumericalPropertyIndex::HP], kMaxHp,
                 "T7225 本体行：体力面板不动");
        std::cout << "  [T7225] 本体行面板护栏 通过\n";
    }

    // ══ T7226 boss：tips 无尾注 → 双支照常 ════════════════════════════════════
    {
        sbs_srand(7226);
        ScenarioBattle b(make_party(mark_pet({3, 2, 1, 3, 300})), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        use_round(b);
        CHECK_EQ(foe_negative_slots(b), 2, "T7226 boss：降属支照常");
        b.ctx().apply_abnormal_status_for_rounds(0, static_cast<int>(AbnormalStatusId::Burn),
                                                 9);
        use_round(b);
        CHECK_EQ(holder_shield(b), 300, "T7226 boss：护盾支照常");
        std::cout << "  [T7226] boss 双支照常 通过\n";
    }

    std::cout << "scenario_7220: all green" << std::endl;
    return 0;
}
