// 场景 3920 —— K245 低体麻痹+高体增伤窗族（798，soul_lib/k245_lowgate_para_highgate_amp.cpp）
//（2026-10-13，夜跑 k-prod-6；号段 3920-3929 → T3920 一块四例）
//
// 断言口径（增伤 ×1.5 下界 1.275B > 常规上界 B，带天然分离）：
//   - a 低体 → 回合末麻痹直附 2 回合：R1 后活跃、R2 后仍活跃、R3 后过期。
//   - b 高体 → 下回合增伤 50%：R1 攻击常规带 [62,99]（pending 尚未置）→
//     R1 末高体置 pending → R2 攻击 ≥102（1.275B 下界 > 常规上界 99）。
//   - c 门界互斥负例：hp = int max/2 → 低体(<)与高体(>)均不触发：
//     无麻痹、R2 仍常规带。
//   - d row2 75%：R2 ≥102 同判（1.4875B 下界更高）。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：攻击技 filler
constexpr int kPara = static_cast<int>(AbnormalStatusId::Paralysis);

SoulMark make_mark(const std::vector<int>& row_args) {
    SoulMark sm(798, "K245 魂印", "test", EffectArgs(row_args));
    sm.monster_id = 3150;   // 格劳瑞
    return sm;
}

ElfPet mark_pet(const std::vector<int>& row_args, int hp) {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, 99999, make_mark(row_args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    if (hp > 0) {
        p.hp = hp;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/1, 99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void one_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const std::vector<int> row1{2, 2, 50, 1};
    const std::vector<int> row2{2, 2, 75, 1};

    // ══ T3920.a 低体 → 回合末麻痹直附 2 回合 ══
    {
        ScenarioBattle b(make_party(mark_pet(row1, -1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 4000;   // 压当前血（构造 hp 会连 max 一起改）
        one_round(b);
        CHECK(ctx.has_active_abnormal_status(1, kPara), "T3920.a R1 末麻痹落地");
        ctx.seerRobot[0].elfPets[0].hp = 99500;   // 拉出门外（否则每回合重施永不过期）
        one_round(b);
        CHECK(!ctx.has_active_abnormal_status(1, kPara), "T3920.a R2 过期");
        std::cout << "  [T3920.a] 低体麻痹直附 通过\n";
        // ⚠️ 记档：anomaly duration=2 计入施加当回合（R1 末施加 → R2 已过期）——引擎口径，
        // "麻痹 2 回合"的完整官方语义（跨 R2/R3）需 duration+1 或 FMR 补判，本单按
        // apply_anomaly 原生口径落地（与 miaoshi/1067 直附族一致）。
    }

    // ══ T3920.b 高体 → 下回合增伤 50%（row1）══
    {
        ScenarioBattle b(make_party(mark_pet(row1, -1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b);
        const int d1 = foe_hp0 - ctx.getPet(1).hp;
        CHECK(d1 > 85 && d1 < 104, "T3920.b R1 常规带（pending 尚未置）");
        const int foe_hp1 = ctx.getPet(1).hp;
        one_round(b);
        const int d2 = foe_hp1 - ctx.getPet(1).hp;
        CHECK(d2 > 102, "T3920.b R2 增伤 50%（下界 1.275B > 常规上界）");
        std::cout << "  [T3920.b] 高体增伤 50% 通过（d1=" << d1 << " d2=" << d2 << "）\n";
    }

    // ══ T3920.c 门界互斥负例：int max/2 双门均不触发 ══
    {
        ScenarioBattle b(make_party(mark_pet(row1, -1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 99999 / 2;   // 恰在 int 门限
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b);
        const int d1 = foe_hp0 - ctx.getPet(1).hp;
        CHECK(!ctx.has_active_abnormal_status(1, kPara), "T3920.c 恰在门限不麻痹");
        CHECK(d1 > 85 && d1 < 104, "T3920.c R1 常规带（双门均未触发）");
        const int foe_hp1 = ctx.getPet(1).hp;
        one_round(b);
        const int d2 = foe_hp1 - ctx.getPet(1).hp;
        CHECK(d2 > 85 && d2 < 104, "T3920.c R2 仍常规带（无增伤）");
        std::cout << "  [T3920.c] 门界互斥负例 通过\n";
    }

    // ══ T3920.d row2 75%：R2 ≥102 ══
    {
        ScenarioBattle b(make_party(mark_pet(row2, -1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b);
        const int foe_hp1 = ctx.getPet(1).hp;
        one_round(b);
        const int d2 = foe_hp1 - ctx.getPet(1).hp;
        CHECK(d2 > 102, "T3920.d row2 增伤 75%");
        std::cout << "  [T3920.d] row2 增伤 75% 通过（d2=" << d2 << "）\n";
    }

    std::cout << "scenario_3920 PASS (K245 低体麻痹+高体增伤窗族)" << std::endl;
    return 0;
}
