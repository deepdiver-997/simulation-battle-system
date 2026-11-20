// 场景 5950 —— J264 能力下降吸取族（1225，moves_lib/j_drain_on_stat_drop.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5950-5959 → T5950.a-d 四块）
//
// 断言口径：
//   - 载体 缚影溃光诀 36928 [1982,1177,1225]|[3]：1225 分母=3 → 吸取 99999/3=33333。
//   - 门开 = 对手六槽任一负等级；门闭 = 全 0 / 仅正等级 / **自身**负等级（对手干净）。
//   - 红伤浮动：用 ctx.ws.raw_attack_damage[0] 读出本次真实红伤，再断言
//     对手掉血 = raw + 33333（精确）、自身回血 = 33333（半血构造）；
//     门闭块断言对手掉血 = raw、自身 HP 绝对值不变。
//   - 审计槽 991225200+owner = 名义吸取量（0=门闭）。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kFuYing = 36928;   // 缚影溃光诀：特 [1982,1177,1225]|[3]
constexpr int kPaiDa  = 10004;   // 拍打：filler
constexpr int kMaxHp  = 99999;
constexpr int kDrain  = 33333;   // 99999 / 3

constexpr int kAuditBase = 991225200;   // +owner

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

ElfPet self_pet() {
    ElfPet p = make_pet(1, "己方", {kFuYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1000, kMaxHp);
    always_hit(p);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    always_hit(p);
    return p;
}

void self_skill(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5950.a 门开：对手防御 -1 → 红伤之外额外吸取 33333，自身半血回 33333 ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 20000;                  // 半血构造（吃 heal 不封顶）
        ctx.ability_levels[1][2] = -1;             // 对手防御下降
        self_skill(b);
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T5950.a 前置：红伤落地");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp - raw - kDrain,
                 "T5950.a 1225：对手掉血 = 红伤 + max/3=33333");
        CHECK_EQ(ctx.getPet(0).hp, 20000 + kDrain,
                 "T5950.a 1225：自身回 33333（drain_hp 回等量）");
        CHECK_EQ(storage_int(ctx, kAuditBase + 0, -1), kDrain,
                 "T5950.a 1225：审计名义吸取量");
        std::cout << "  [T5950.a] 1225 门开 通过\n";
    }

    // ══ T5950.b 门闭（全 0）：无负等级 → 只掉红伤、不回血 ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 20000;
        self_skill(b);
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T5950.b 前置：红伤落地");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp - raw,
                 "T5950.b 1225：门闭 → 无额外吸取（绝对值）");
        CHECK_EQ(ctx.getPet(0).hp, 20000, "T5950.b 1225：门闭 → 自身不回血");
        CHECK_EQ(storage_int(ctx, kAuditBase + 0, -1), 0, "T5950.b 1225：审计=0");
        std::cout << "  [T5950.b] 1225 门闭（全 0） 通过\n";
    }

    // ══ T5950.c 门闭（仅正等级）：+1 不等于能力下降 → 不吸取 ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 20000;
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            ctx.ability_levels[1][i] = 1;   // 对手全 +1（能力提升，不是下降）
        }
        self_skill(b);
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T5950.c 前置：红伤落地");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp - raw,
                 "T5950.c 1225：仅正等级 → 门闭");
        CHECK_EQ(ctx.getPet(0).hp, 20000, "T5950.c 1225：不吸取 → 自身不回血");
        std::cout << "  [T5950.c] 1225 门闭（仅正等级） 通过\n";
    }

    // ══ T5950.d 门读对手而非自身：自身 -1、对手干净 → 门闭 ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 20000;
        ctx.ability_levels[0][0] = -1;   // 自身攻击下降（对手仍 0）
        self_skill(b);
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T5950.d 前置：红伤落地");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp - raw,
                 "T5950.d 1225：自身下降不计入门（对手干净）");
        CHECK_EQ(ctx.getPet(0).hp, 20000, "T5950.d 1225：不吸取");
        std::cout << "  [T5950.d] 1225 门侧位 通过\n";
    }

    std::cout << "scenario_5950 PASS (J264 能力下降吸取族)" << std::endl;
    return 0;
}
