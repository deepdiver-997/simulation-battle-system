// 场景 2880 —— K163 开场/登场免疫恢复族（221/270/261，soul_lib/k_open_window_immune.cpp）
//（2026-10-05，worktree ../agents/k163；号段 2880-2889 → T2880-T2882 三块）
//
// 断言口径（全部直写 setup + 直查状态，无随机）：
//   - 221：入场免异常 1 回合——R1 对手直铺烧伤被免（抗性/异常表双查）→ R2 免疫过期
//     再铺 → 落地；
//   - 270：入场免降 2 回合——R1/R2 直铺 stat_drop 被免（levels 不动）→ R3 过期 →
//     -1 落地；
//   - 261：每回合 ROUND_START 回 max/10——hp 压 5000 → R1 start +9999（int 整除）
//     → 回血封顶 max。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：filler

constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2

template <int MarkId>
ElfPet mark_pet(const std::array<int, 5>& skills, int speed = 300, int hp = 99999) {
    ElfPet p = make_pet(1, "印主", skills, speed, /*atk=*/1, hp,
                        /*soul_mark=*/SoulMark(MarkId, "K163 魂印", ""));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(const std::array<int, 5>& skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void both_act(ScenarioBattle& b, int self_slot = 0, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2880 221 首回合免疫异常（exp: R1 直铺被免 → R2 过期落地）══
    {
        ScenarioBattle b(make_party(mark_pet<221>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(!try_apply_anomaly(&ctx, 0, kBurn, /*duration=*/3, /*actor=*/1),
              "T2880 221：首回合免疫 → 直铺被拒（apply 返回 false）");
        CHECK(!ctx.has_active_abnormal_status(0, kBurn),
              "T2880 221：首回合免疫所有异常（R1 无异常）");
        both_act(b, 0);   // R1 推进（rc 0→1，免疫窗过期）
        REQUIRE(try_apply_anomaly(&ctx, 0, kBurn, /*duration=*/3, /*actor=*/1));
        CHECK(ctx.has_active_abnormal_status(0, kBurn),
              "T2880 221：免疫窗过期（R2 起）→ 烧伤落地");
        std::cout << "  [T2880] 221 免疫异常 通过\n";
    }

    // ══ T2881 270 登场 2 回合免降（R1/R2 直铺被免 → R3 过期落地）══
    {
        ScenarioBattle b(make_party(mark_pet<270>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_drop(&ctx, 0, 0, 1, 1) == StatDropResult::IMMUNE);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T2881 270：R1 免降（直铺被免）");
        both_act(b, 0);   // R1 推进
        REQUIRE(stat_drop(&ctx, 0, 0, 1, 1) == StatDropResult::IMMUNE);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T2881 270：R2 仍免降（窗=2 回合）");
        both_act(b, 0);   // R2 推进（rc→2，窗过期）
        REQUIRE(stat_drop(&ctx, 0, 0, 1, 1) == StatDropResult::SUCCESS);
        CHECK_EQ(ctx.ability_levels[0][0], -1, "T2881 270：R3 过期 → -1 落地");
        std::cout << "  [T2881] 270 免降 通过\n";
    }

    // ══ T2882 261 每回合 ROUND_START 回 max/10（int 整除）══
    {
        ScenarioBattle b(make_party(mark_pet<261>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;
        // b.start() 停在 R1 选技态；R1 的 ROUND_START 在 drive 内——对手 NONE 免干扰
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 14999, "T2882 261：回合开始回 max/10=9999");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 24998, "T2882 261：每回合结算（14999+9999）");
        std::cout << "  [T2882] 261 每回合恢复 通过\n";
    }

    std::cout << "scenario_2880 PASS (K163 开场/登场免疫恢复族)" << std::endl;
    return 0;
}
