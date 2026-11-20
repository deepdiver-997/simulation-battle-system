#include <utils/sbs_rand.h>
// 场景 949 —— K2 概率递增（effect 662 达赫迪/小卷）：爬坡 + F9 审计槽无随机梯度断言
//
// 机制口径：攻击类技能使用时，pct = min(base + 已损体力/档 × 步, 上限)——爬**自身**
// 已损体力（小卷行"自身每失去100点体力"明确主语）；申报概率写审计槽 990670000+owner。
// 断言口径：审计槽断**申报值**（梯度精确、与 roll 无关）；落地断言用 base=100 档。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中
constexpr int kGaoSu = 20017;    // 高速移动：属性技
constexpr int kAuditKeyBase = 990670000;

constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Crippled);   // 19 瘫痪（限制类）
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);      // 7 疲惫

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

// 攻一回合；可选把 0 方体力扣成 max-lost_before。返回 {1 方是否瘫痪, 审计槽申报值}。
struct ProbeResult {
    bool landed = false;
    int declared = -1;
};

ProbeResult probe(const SoulMark& mark, int lost_before, int status_to_check) {
    constexpr int kMaxHp = 1000000;
    ElfPet holder = scenario::make_pet(1, "达赫迪", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/5000, /*hp=*/kMaxHp, mark);
    ScenarioBattle b(scenario::make_party(holder),
                     scenario::make_party(scenario::make_pet(
                         2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/kMaxHp)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20260916);
    b.start();
    if (lost_before > 0) {
        ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = kMaxHp - lost_before;
    }
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    ProbeResult r;
    r.landed = ctx.has_active_abnormal_status(1, status_to_check);
    const auto it = ctx.plugin_storage.find(kAuditKeyBase + 0);
    r.declared = (it != ctx.plugin_storage.end()) ? std::any_cast<int>(it->second) : -1;
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // 达赫迪行骨架 [1, base, 19, 50, 5, 50]；场景按需调 base/cap 做确定性断言。
    // ══ A 满体力 + base=0 → 申报 0（不爬坡不触发）═══════════════════════════
    {
        const auto r = probe(make_mark(662, {1, 0, kParalysis, 50, 5, 100}, 2940), 0, kParalysis);
        CHECK_EQ(r.declared, 0, "A 满体力 base=0 → 申报 0");
        CHECK(!r.landed, "A 申报 0 → 不瘫痪");
    }
    // ══ B 已损 120 体力（档 50）→ 申报 0 + 2×5 = 10 ═════════════════════════
    {
        const auto r = probe(make_mark(662, {1, 0, kParalysis, 50, 5, 100}, 2940), 120, kParalysis);
        CHECK_EQ(r.declared, 10, "B 已损 120/50=2 档 → 申报 10");
    }
    // ══ C 爬坡封顶：已损 10000 → min(0+200×5, 100) = 100 ═══════════════════
    {
        const auto r = probe(make_mark(662, {1, 0, kParalysis, 50, 5, 100}, 2940), 10000, kParalysis);
        CHECK_EQ(r.declared, 100, "C 爬坡封顶 → 申报 100");
    }
    // ══ D base=100 满体力 → 落地（瘫痪 19，限制类建模照做）══════════════════
    {
        const auto r = probe(make_mark(662, {1, 100, kParalysis, 50, 5, 100}, 2940), 0, kParalysis);
        CHECK_EQ(r.declared, 100, "D 申报 100");
        CHECK(r.landed, "D 瘫痪落地");
    }
    // ══ E 小卷行（疲惫 7，档 100 步 10）：已损 250 → 申报 min(0+2×10,100)=20 ═
    {
        const auto r = probe(make_mark(662, {1, 0, kFatigue, 100, 10, 100}, 4610), 250, kFatigue);
        CHECK_EQ(r.declared, 20, "E 小卷档 100/步 10 → 已损 250 申报 20");
    }

    std::cout << "scenario_949 PASS (K2 662 概率递增：审计槽梯度 + 封顶 + 落地)" << std::endl;
    return 0;
}
