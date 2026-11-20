#include <utils/sbs_rand.h>
// 场景 6020 —— J271 每回合概率封属兜底族（1458/1622，moves_lib/j_chance_seal_fallback.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6020-6029 → T6020.a-d 四块）
//
// 断言口径：
//   - 1458 纯链 36400 祖传秘方 [3,50,50,29] / 36516 迷离圣光 [3,50,100,8]；
//     1622 纯链 36677 杀意威慑 [2,50,1,50]。
//   - 50% 掷点用 sbs_srand 固定序列：seed 1 首掷 7（<50 命中封属）；
//     seed 8 首掷 56（≥50 未触发兜底）。首掷即本效果的 roll（必中/零暴击短路）。
//   - 封属可观测面 = 对手属性技 缩头 20001（自身防御+1）：被封 → 防御等级 0；
//     未封 → 防御等级 +1。
//   - 1458 兜底 = 100% 睡眠(8)；1622 兜底 = 1 次受伤 -50%（对手拍打 atk200 红伤带
//     [68,80] → 减半后 [34,40]，门闭对照 ≥60）。
#include <any>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kZuChuan = 36400;   // 祖传秘方 [1458]|[3,50,50,29]（物130）
constexpr int kMiLi    = 36516;   // 迷离圣光 [1458]|[3,50,100,8]（特140）
constexpr int kShaYi   = 36677;   // 杀意威慑 [1622]|[2,50,1,50]（特120）
constexpr int kSuo     = 20001;   // 缩头：属性技，自身防御+1
constexpr int kPaiDa   = 10004;   // 拍打：物40
constexpr int kMaxHp   = 99999;

constexpr int kAudit1458Seal = 991458200;
constexpr int kAudit1458Anom = 991458300;
constexpr int kAudit1622Arm  = 991622200;
constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);

int storage_int(BattleContext& ctx, int key, int fallback) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : fallback;
}

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(int first_skill) {
    ElfPet p = make_pet(1, "己方", {first_skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1000, kMaxHp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(int first_skill, int atk = 1) {
    ElfPet p = make_pet(2, "对手", {first_skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, kMaxHp);
    always_hit(p);
    return p;
}

void self_first(ScenarioBattle& b, int seed, BattleFsm::ActionType foe_choice, int foe_slot) {
    sbs_srand(seed);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, foe_choice, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6020.a 1458 掷中：封属性 → 对手缩头无效（防御等级 0）══
    {
        ScenarioBattle b(make_party(self_pet(kZuChuan)),
                         make_party(foe_pet(kSuo)));
        b.start();
        BattleContext& ctx = b.ctx();
        self_first(b, /*seed=*/1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(storage_int(ctx, kAudit1458Seal + 0, -1), 1,
                 "T6020.a 1458：首掷命中 → 封属武装");
        CHECK_EQ(storage_int(ctx, kAudit1458Anom + 0, -1), 0,
                 "T6020.a 1458：命中封属 → 不走兜底");
        CHECK_EQ(ctx.ability_levels[1][2], 0,
                 "T6020.a 1458：对手属性技缩头被封 → 防御等级 0");
        std::cout << "  [T6020.a] 1458 封属命中 通过\n";
    }

    // ══ T6020.b 1458 掷歪：兜底 100% 睡眠(8) ══
    {
        ScenarioBattle b(make_party(self_pet(kMiLi)),
                         make_party(foe_pet(kPaiDa)));
        b.start();
        BattleContext& ctx = b.ctx();
        self_first(b, /*seed=*/8, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(storage_int(ctx, kAudit1458Seal + 0, -1), 0,
                 "T6020.b 1458：首掷未中 → 未封属");
        CHECK_EQ(storage_int(ctx, kAudit1458Anom + 0, -1), 1,
                 "T6020.b 1458：兜底 100% 异常落地");
        CHECK(ctx.has_active_abnormal_status(1, kSleep),
              "T6020.b 1458：对手睡眠(8)在身");
        std::cout << "  [T6020.b] 1458 兜底异常 通过\n";
    }

    // ══ T6020.c 1622 掷歪：下 1 次受伤 -50%（对手拍打红伤减半）══
    {
        ScenarioBattle b(make_party(self_pet(kShaYi)),
                         make_party(foe_pet(kPaiDa, /*atk=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        self_first(b, /*seed=*/8, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(storage_int(ctx, kAudit1622Arm + 0, -1), 1,
                 "T6020.c 1622：首掷未中 → 减伤票武装");
        const int taken = ctx.resolvedDamage.final;
        CHECK(taken >= 25 && taken <= 45,
              "T6020.c 1622：1 次受伤 -50%（实测 " + std::to_string(taken) +
                  "，未减伤带 [68,80]）");
        std::cout << "  [T6020.c] 1622 兜底减伤 通过 (taken=" << taken << ")\n";
    }

    // ══ T6020.d 1622 掷中：封属不武装减伤票（对手物理攻击全额）══
    {
        ScenarioBattle b(make_party(self_pet(kShaYi)),
                         make_party(foe_pet(kPaiDa, /*atk=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        self_first(b, /*seed=*/1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(storage_int(ctx, kAudit1622Arm + 0, -1), 0,
                 "T6020.d 1622：首掷命中封属 → 减伤票不武装");
        const int taken = ctx.resolvedDamage.final;
        CHECK(taken >= 60,
              "T6020.d 1622：无减伤票 → 对手物理攻击全额（实测 " +
                  std::to_string(taken) + "）");
        std::cout << "  [T6020.d] 1622 命中封属不兜底 通过 (taken=" << taken << ")\n";
    }

    std::cout << "scenario_6020 PASS (J271 每回合概率封属兜底族)" << std::endl;
    return 0;
}
