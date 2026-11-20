// 场景 5930 —— J262 害怕兜底六槽弱化族（504，moves_lib/j_fear_fallback_sixslot.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5930-5939）
//
// 断言口径（审计槽 + 对手六槽等级精确快照 + 两条确定性驱动）：
//   - 未触发分支用**异常免疫票**强制（必被挡，与掷点无关）；落地分支用**概率闸门**
//     把申报概率改写为 100（grant_chance_gate：threshold=0 / above=100，只管辖
//     Skill 来源）——两支都无随机，不靠种子。
//   - 22456 凄凉之地 [504]|[50,-1,0,-1,0,0,0]：官方"攻击、特攻-1" →
//     引擎槽 [0]=-1（攻）、[1]=-1（特攻）、[2]=0（防）、其余 0。**锁 1/2 换位**
//     （直读官方序会得到 [1]=0/[2]=-1）。
//   - 23822 神灵洗礼 [504]|[80,0,0,0,0,-2,-2]：官方"速度、命中-2" →
//     引擎槽 [4]=-2、[5]=-2，其余 0。
//   - 落地分支：害怕挂上（has_active_abnormal_status(1,6)）、六槽全 0（不走兜底）。
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;      // 拍打（对手 NONE 时不用，仅占位）
constexpr int kCarrierMap = 22456; // 凄凉之地 [504]|[50,-1,0,-1,0,0,0]
constexpr int kCarrierSpd = 23822; // 神灵洗礼 [504]|[80,0,0,0,0,-2,-2]
constexpr int kMaxHp = 99999;
constexpr int kFear = 6;
constexpr int kImmuneSource = 7902;
constexpr int kGateSource = 7903;
constexpr int kLandedAuditBase = 996504100;

ElfPet mk_pet(int id, const std::string& name, int carrier, int speed) {
    ElfPet p = make_pet(id, name, {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/speed, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
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

struct Result {
    int landed = -999;
    bool fear = false;
    std::array<int, 6> slots{0, 0, 0, 0, 0, 0};
};

// 一回合对局：己方施放 carrier；mode 0 = 免疫票（强制未触发）/ 1 = 概率闸门（强制落地）。
Result run_once(int carrier, int mode) {
    ScenarioBattle b(make_party(mk_pet(1, "己方", carrier, /*speed=*/300)),
                     make_party(mk_pet(2, "对手", kPaiDa, /*speed=*/100)));
    b.start();
    BattleContext& ctx = b.ctx();
    if (mode == 0) {
        ctx.grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/0,
                           /*source_id=*/kImmuneSource, /*soul_immunity=*/true);
    } else {
        ctx.grant_chance_gate(/*owner=*/1, kGateSource, ChanceTag::AnomalyAttach,
                              /*threshold_pct=*/0, /*below_result=*/-1, /*above_result=*/100,
                              /*source_mask=*/chance_source_bit(ChanceSource::Skill),
                              /*rounds=*/0);
    }
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();

    Result r;
    r.landed = audit(ctx, kLandedAuditBase + 0);
    r.fear = ctx.has_active_abnormal_status(1, kFear);
    for (int i = 0; i < 6; ++i) {
        r.slots[static_cast<std::size_t>(i)] = ctx.ability_levels[1][i];
    }
    return r;
}

void check_slots(const Result& r, const std::array<int, 6>& want, const char* label) {
    for (int i = 0; i < 6; ++i) {
        CHECK_EQ(r.slots[static_cast<std::size_t>(i)], want[static_cast<std::size_t>(i)],
                 std::string(label) + " 对手槽 " + std::to_string(i));
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A：凄凉之地 + 强制未触发 → 攻/特攻 -1（引擎槽 0/1），锁 1/2 换位 ══
    {
        const Result r = run_once(kCarrierMap, /*mode=*/0);
        CHECK_EQ(r.landed, 0, "A1 免疫票挡下 → 落地审计=0");
        CHECK(!r.fear, "A2 害怕未挂上");
        check_slots(r, {{-1, -1, 0, 0, 0, 0}}, "A3");
        std::cout << "  [A] 兜底六槽：攻-1/特攻-1（槽0/1），防=0（槽2）\n";
    }

    // ══ B：凄凉之地 + 概率闸门强制落地 → 害怕挂上、六槽全 0 ══
    {
        const Result r = run_once(kCarrierMap, /*mode=*/1);
        CHECK_EQ(r.landed, 1, "B1 闸门改写为 100% → 落地审计=1");
        CHECK(r.fear, "B2 害怕挂上");
        check_slots(r, {{0, 0, 0, 0, 0, 0}}, "B3");
        std::cout << "  [B] 主句落地：害怕挂上、无兜底弱化\n";
    }

    // ══ C：神灵洗礼 + 强制未触发 → 速度/命中 -2（引擎槽 4/5） ══
    {
        const Result r = run_once(kCarrierSpd, /*mode=*/0);
        CHECK_EQ(r.landed, 0, "C1 免疫票挡下 → 落地审计=0");
        CHECK(!r.fear, "C2 害怕未挂上");
        check_slots(r, {{0, 0, 0, 0, -2, -2}}, "C3");
        std::cout << "  [C] 兜底六槽：速度-2/命中-2（槽4/5）\n";
    }

    // ══ D：神灵洗礼 + 概率闸门强制落地 → 害怕挂上、六槽全 0 ══
    {
        const Result r = run_once(kCarrierSpd, /*mode=*/1);
        CHECK_EQ(r.landed, 1, "D1 闸门改写为 100% → 落地审计=1");
        CHECK(r.fear, "D2 害怕挂上");
        check_slots(r, {{0, 0, 0, 0, 0, 0}}, "D3");
        std::cout << "  [D] 主句落地：害怕挂上、无兜底弱化\n";
    }

    std::cout << "scenario_5930 PASS (J262 害怕兜底六槽弱化族)" << std::endl;
    return 0;
}
