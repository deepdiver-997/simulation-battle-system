// 场景 6210 —— J288 护盾/护罩转固伤族（1662 / 1537，moves_lib/j_shield_burst_heal.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6210-6219）
//
// 断言口径：
//   - 两宏都用**属性技**载体（28939 钢铁雄心 / 28116 残躯不腐）→ 无红伤，hp 变化只可能
//     来自被测固伤/恢复，绝对量可精确断言。
//   - 1662：护盾 800 → 消耗 800 + 对手固伤 800；对手有厚护罩吃下固伤 → "对手体力未变化"
//     → 自身恢复 800（半血观测防 clamp；另测 4800 血量下的实际恢复 200）。
//   - 1537：护罩 2000（>350）→ 固伤 2000；200（<350）→ 固伤 400（翻倍）；350 → 严格
//     "低于"不成立 → 固伤 350；0 → 无事。阈值 = 28116 切片 {0}=350。
//   - 审计槽记录"本次实际消耗/固伤/恢复"（无随机），与 hp 绝对值双向核对。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;    // 拍打：物 40（填充刀）
constexpr int kGangTie = 28939;  // 钢铁雄心 属技 [48,47,2050,1655,1662]|[5,5,1,1,3,5,0,1]
constexpr int kCanQu = 28116;    // 残躯不腐 属技 [191,426,1394,1537]|[4,3,100,3,2,350]

constexpr int k1662Consumed = 991662000;
constexpr int k1662Heal = 991662100;
constexpr int k1537Consumed = 991537000;
constexpr int k1537Damage = 991537100;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(int skill) {
    ElfPet p = make_pet(1, "己方", {skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/0, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/10000);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : -999999;   // 缺失与显式 0 区分（前置校验）
}

void run(ElfPet self, ElfPet foe) {
    ScenarioBattle b(make_party(self), make_party(foe));
    b.start();
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

    // ══ T6210 1662 护盾转固伤 / 未变化回血 ══
    {
        {   // (a) 护盾 800，对手无护罩 → 固伤 800 落地，不回血
            ScenarioBattle b(make_party(mk_self(kGangTie)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 3000;
            ctx.getPet(0).shield_bank_.add(0, 800, 9001, 0);
            ctx.getPet(0).hood_bank_.add(0, 500, 9002, 0);   // 护罩不应被 1662 动
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(stor(ctx, k1662Consumed), 800, "T6210.a 1662：消耗护盾 800");
            CHECK_EQ(stor(ctx, k1662Heal), 0, "T6210.a 1662：固伤落地 → 不回血");
            CHECK_EQ(ctx.getPet(1).hp, 9200, "T6210.a 1662：对手 10000→9200（固伤 800）");
            CHECK_EQ(ctx.getPet(0).hp, 3000, "T6210.a 1662：自身 hp 不变");
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0, "T6210.a 1662：护盾清空");
            CHECK_EQ(ctx.getPet(0).hood_bank_.total(), 500,
                     "T6210.a 1662：护罩不被 1662 消耗（只动护盾账）");
        }
        {   // (b) 对手有护罩 2000 → 固伤被吃光、对手 hp 未变化 → 自身回 800
            ScenarioBattle b(make_party(mk_self(kGangTie)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 3000;
            ctx.getPet(0).shield_bank_.add(0, 800, 9001, 0);
            ctx.getPet(1).hood_bank_.add(0, 2000, 9003, 0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(stor(ctx, k1662Consumed), 800, "T6210.b 1662：消耗护盾 800");
            CHECK_EQ(stor(ctx, k1662Heal), 800, "T6210.b 1662：对手体力未变化 → 回 800");
            CHECK_EQ(ctx.getPet(0).hp, 3800, "T6210.b 1662：自身 3000→3800");
            CHECK_EQ(ctx.getPet(1).hp, 10000, "T6210.b 1662：对手 hp 未变化");
            CHECK_EQ(ctx.getPet(1).hood_bank_.total(), 1200,
                     "T6210.b：对手护罩 2000→1200（吃下 800 固伤）");
        }
        {   // (c) 自身无护盾 → 整条无事（不产生 0 伤、不回血）
            ScenarioBattle b(make_party(mk_self(kGangTie)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 3000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(stor(ctx, k1662Consumed), 0, "T6210.c 1662：无盾 → 消耗 0");
            CHECK_EQ(stor(ctx, k1662Heal), 0, "T6210.c 1662：无盾 → 不回血");
            CHECK_EQ(ctx.getPet(1).hp, 10000, "T6210.c 1662：对手 hp 不变");
        }
        {   // (d) 回血被最大体力 clamp：hp 4800 + 800 → 实际恢复 200
            ScenarioBattle b(make_party(mk_self(kGangTie)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 4800;
            ctx.getPet(0).shield_bank_.add(0, 800, 9001, 0);
            ctx.getPet(1).hood_bank_.add(0, 2000, 9003, 0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(stor(ctx, k1662Heal), 200, "T6210.d 1662：实际恢复量 200（clamp）");
            CHECK_EQ(ctx.getPet(0).hp, 5000, "T6210.d 1662：自身钳满 5000");
        }
        std::cout << "  [T6210] 1662 护盾转固伤/未变化回血 通过\n";
    }

    // ══ T6210 1537 护罩转固伤 / 低值翻倍阈值 ══
    {
        {   // (a) 护罩 2000 > 350 → 固伤 2000（不翻倍）
            ScenarioBattle b(make_party(mk_self(kCanQu)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hood_bank_.add(0, 2000, 9101, 0);
            ctx.getPet(0).shield_bank_.add(0, 500, 9102, 0);   // 护盾不应被 1537 动
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(stor(ctx, k1537Consumed), 2000, "T6210.e 1537：消耗护罩 2000");
            CHECK_EQ(stor(ctx, k1537Damage), 2000, "T6210.e 1537：2000 不 < 350 → 不翻倍");
            CHECK_EQ(ctx.getPet(1).hp, 8000, "T6210.e 1537：对手 10000→8000");
            CHECK_EQ(ctx.getPet(0).hood_bank_.total(), 0, "T6210.e 1537：护罩清空");
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 500,
                     "T6210.e 1537：护盾不被 1537 消耗（只动护罩账）");
        }
        {   // (b) 护罩 200 < 350 → 固伤 400（翻倍）
            ScenarioBattle b(make_party(mk_self(kCanQu)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hood_bank_.add(0, 200, 9101, 0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(stor(ctx, k1537Consumed), 200, "T6210.f 1537：消耗护罩 200");
            CHECK_EQ(stor(ctx, k1537Damage), 400, "T6210.f 1537：200 < 350 → 固伤翻倍 400");
            CHECK_EQ(ctx.getPet(1).hp, 9600, "T6210.f 1537：对手 10000→9600");
        }
        {   // (c) 边界：护罩恰 350 → "低于"严格不成立 → 不翻倍
            ScenarioBattle b(make_party(mk_self(kCanQu)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hood_bank_.add(0, 350, 9101, 0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(stor(ctx, k1537Damage), 350, "T6210.g 1537：阈值相等不翻倍（严格低于）");
            CHECK_EQ(ctx.getPet(1).hp, 9650, "T6210.g 1537：对手 10000→9650");
        }
        {   // (d) 无护罩 → 无事
            ScenarioBattle b(make_party(mk_self(kCanQu)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(stor(ctx, k1537Consumed), 0, "T6210.h 1537：无护罩 → 消耗 0");
            CHECK_EQ(stor(ctx, k1537Damage), 0, "T6210.h 1537：无护罩 → 不产生固伤");
            CHECK_EQ(ctx.getPet(1).hp, 10000, "T6210.h 1537：对手 hp 不变");
        }
        std::cout << "  [T6210] 1537 护罩转固伤/低值翻倍 通过\n";
    }

    std::cout << "scenario_6210_shield_burst_heal PASS" << std::endl;
    return 0;
}
