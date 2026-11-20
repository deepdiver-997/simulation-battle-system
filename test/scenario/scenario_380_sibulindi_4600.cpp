#include <utils/sbs_rand.h>
// 场景 380：斯布林蒂 4600 五技 彼空·蔚莱唤歌（2110/1740/1821；761 已有不在本测面）
// （2026-09-28，night-3 / D22；实现 resources/moves_lib/d5_D22.cpp）
//
// T1 1740：两项 PP 不满 → 固定伤 300 落地（守方血差 ≥300）。
// T2 1740 免粉：守方 PINK_DAMAGE 免疫 → 固定伤 0，追加真伤 200（血差 ≥200 且 <300）。
// T3 1821：未击败 → 全技能 +5 PP（37094 4→5、预置 10→15）。
// T4 2110：背包链接精灵（暗影系、2 级）与守方同系别 → 克制覆盖
//      natural+bonus：彼空 37094 系别 121=["nature","psychic"]（复合技），J322 后
//      natural=avg(自然→暗影 0.5, 超能→暗影 1.0)=0.75（原复合 id 直查矩阵落缺省 1.0）
//      + 0.5×2 = 1.75。
// T5 2110 负例：链接但系别不合 → 不写覆盖槽（restraint_view 保持 -1）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <effects/future_link.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHit = 10004;      // 拍打
constexpr int kWeiLai = 37094;   // 彼空·蔚莱唤歌：[2110,1740,761,1821]
constexpr int kElemDark = 13;    // 暗影

ElfPet sibulindi() {
    ElfPet p = scenario::make_pet(4600, "斯布林蒂", {kWeiLai, kHit, kHit, kHit, kHit},
                                  /*speed=*/200, /*atk=*/20, 100000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = scenario::make_pet(1, "守方", {kHit, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, /*atk=*/0, 100000);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
    return p;
}

void one_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    sbs_srand(20260928);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 1740：两项 PP 不满 → 固定伤 300 ==========
    {
        ScenarioBattle b(scenario::make_party(sibulindi()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].skills[0].pp -= 1;   // 37094 本身不满（不计：第五技）
        ctx.seerRobot[0].elfPets[0].skills[1].pp = 34;   // 槽 1 不满（拍打 maxPP=35）
        ctx.seerRobot[0].elfPets[0].skills[2].pp = 33;   // 槽 2 不满
        const int before = ctx.getPet(1).hp;
        one_round(b);
        const int delta = before - ctx.getPet(1).hp;
        CHECK(delta >= 300, "T1 两项 PP 不满：固定伤 300 落地（含守方反击红伤）");
        std::cout << "  [T1] 1740 PP 不满固定伤 通过（血差 " << delta << "）\n";
    }

    //========== T2 1740 免粉：固定伤被免 → 追加 200 真伤 ==========
    {
        ScenarioBattle b(scenario::make_party(sibulindi()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].skills[1].pp = 34;
        ctx.seerRobot[0].elfPets[0].skills[2].pp = 33;
        ctx.grant_immunity(1, ImmunityType::PINK_DAMAGE, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/3, /*source_id=*/999);
        const int before = ctx.getPet(1).hp;
        one_round(b);
        const int delta = before - ctx.getPet(1).hp;
        CHECK(delta >= 200 && delta < 300,
              "T2 固定伤被免 → 真伤 200（<300 区分非免路径的 300+）");
        std::cout << "  [T2] 1740 免粉追加真伤 通过（血差 " << delta << "）\n";
    }

    //========== T3 1821 未击败：全技能 +5 PP ==========
    {
        ScenarioBattle b(scenario::make_party(sibulindi()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].skills[1].pp = 10;
        one_round(b);
        CHECK_EQ(ctx.getPet(0).skills[0].pp, 5, "T3 37094：4→5（+5 钳满）");
        CHECK_EQ(ctx.getPet(0).skills[1].pp, 15, "T3 拍打：10→15");
        std::cout << "  [T3] 1821 全技能 +5 PP 通过\n";
    }

    //========== T4 2110：背包链接同系别 → 克制 2.0 ==========
    {
        ElfPet foe = foe_pet();
        foe.elementalAttributes[0] = kElemDark;   // 守方单暗影
        ScenarioBattle b(scenario::make_party(sibulindi()), scenario::make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[1].elementalAttributes[0] = kElemDark;   // 背包同系别
        future_link_set_level(&ctx, 0, 1, 2);                             // 链接 2 级
        one_round(b);
        CHECK_EQ(static_cast<int>(ctx.ws.restraint_view[0] * 100), 175,
                 "T4 链接共振：natural avg(0.5,1.0)=0.75 + 0.5×2 = 1.75（J322 复合技聚合生效）");
        std::cout << "  [T4] 2110 链接共振克制 通过\n";
    }

    //========== T5 2110 负例：链接但系别不合 → 不覆盖 ==========
    {
        ScenarioBattle b(scenario::make_party(sibulindi()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[1].elementalAttributes[0] = kElemDark;   // 系别不合（守方普通）
        future_link_set_level(&ctx, 0, 1, 3);
        one_round(b);
        CHECK_EQ(static_cast<int>(ctx.ws.restraint_view[0] * 100), -100,
                 "T5 无共振：覆盖槽保持 -1");
        std::cout << "  [T5] 2110 无共振不覆盖 通过\n";
    }

    std::cout << "scenario_380 all passed" << std::endl;
    return 0;
}
