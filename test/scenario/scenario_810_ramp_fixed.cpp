// 场景 810 —— H3 连击/递增固伤族（429 / 162，moves_lib/h_ramp_fixed.cpp）
//（2026-09-30，worktree ../agents/h3；号段 810-819 一文件覆盖两效果、四载体）
//
// 机制：
//   429 附加{0}点固伤，连用每次+{1}，最高{2} —— effect_9/1443 连用范式
//        （skill_use_seq + plugin_storage 连用账本，换技断连）；首用=base；
//   162 对手处于**任意**异常 → 附加{0}点固伤（foe_has_any_anomaly 遍历判据）。
//
// 断言口径（G3 场景同款）：
//   - 粉伤精确断 = 对手总掉血 − ws.raw_attack_damage（红伤浮动被台账实测吸收）
//     − 中毒 tick（对手 ACTION_START 结算 max(1, max_hp/8)，hp=9999 → 1249，
//     确定性并入断言，730 同款）；
//   - 连用序列 R1..Rn 逐回合精确断（25,50,75,100,100 钉 base/step/cap 三参）；
//   - 断连 = 中途换另一载体技（账本技能 id 比对失败）→ 再用回时重置 base；
//   - 162 无异常对照 loss==raw；中毒路 loss−raw−tick==amount。
//   - 载体必中（消 miss 噪声）；对手 NONE 不出手。
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

constexpr int kXingKong = 13535;  // 异次元星空：[429] → [25,25,100]
constexpr int kShuangYa = 16271;  // 鬼神双牙斩：[429] → [70,60,250]
constexpr int kBaoZi    = 12799;  // 孢子碎击炮：[162] → [50]
constexpr int kZheTian  = 17024;  // 遮天光束斩：[162] → [200]
constexpr int kPaDa     = 10004;  // 拍打：filler
constexpr int kFoeHp    = 9999;

constexpr int kPoisonId = static_cast<int>(AbnormalStatusId::Poison);   // 1
constexpr int kPoisonTick = std::max(1, kFoeHp / 8);                    // 1249（确定性）

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int atk = 1) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, atk, /*hp=*/99999);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, kFoeHp);
    always_hit(p);
    return p;
}

void play_round(ScenarioBattle& b, int self_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 粉伤提取：本回合对手掉血 − 红伤台账实测（− 中毒 tick 由调用方并入期望）。
int pink_of(BattleContext& ctx, int hp_before) {
    const int raw = ctx.ws.raw_attack_damage[0];
    const int loss = hp_before - ctx.getPet(1).hp;
    return loss - raw;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // 429 异次元星空 [25,25,100]
    // ════════════════════════════════════════════════════════════════

    // T1 连用序列 R1..R5 = 25,50,75,100,100（base/step/cap 三参全钉）
    {
        ScenarioBattle b(make_party(self_pet({kXingKong, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int expected[5] = {25, 50, 75, 100, 100};
        for (int r = 0; r < 5; ++r) {
            const int hp_before = ctx.getPet(1).hp;
            play_round(b, 0);
            const int pink = pink_of(ctx, hp_before);
            CHECK_EQ(pink, expected[r], "T1 核心：连用第 n 次附加 = min(25+(n-1)*25, 100)");
        }
        std::cout << "  [T1] 429 连用序列 25/50/75/100/100 通过\n";
    }

    // T2 换技断连：星空(25) → 双牙首用(70) → 回到星空重置(25)
    {
        std::array<int, 5> sk = {kXingKong, kShuangYa, kPaDa, kPaDa, kPaDa};
        ScenarioBattle b(make_party(self_pet(sk)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        int hp_before = ctx.getPet(1).hp;
        play_round(b, 0);   // 星空 R1 = 25
        CHECK_EQ(pink_of(ctx, hp_before), 25, "T2 前提：星空首用 25");
        hp_before = ctx.getPet(1).hp;
        play_round(b, 1);   // 双牙 R2 = 首用 70（不同技能，不接星空的账）
        CHECK_EQ(pink_of(ctx, hp_before), 70, "T2 核心：换技 → 新账本首用 70");
        hp_before = ctx.getPet(1).hp;
        play_round(b, 0);   // 星空 R3 = 断连重置 25
        CHECK_EQ(pink_of(ctx, hp_before), 25, "T2 核心：回到星空 → 序号跳变断连 → 重置 25");
        std::cout << "  [T2] 429 换技断连通过\n";
    }

    // T3 第二载体连用第二跳：双牙 70 → 130（min(70+60,250)）
    {
        ScenarioBattle b(make_party(self_pet({kShuangYa, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        int hp_before = ctx.getPet(1).hp;
        play_round(b, 0);
        CHECK_EQ(pink_of(ctx, hp_before), 70, "T3 前提：双牙首用 70");
        hp_before = ctx.getPet(1).hp;
        play_round(b, 0);
        CHECK_EQ(pink_of(ctx, hp_before), 130, "T3 核心：连用第 2 次 = 70+60（cap 250 未到）");
        std::cout << "  [T3] 429 第二载体（鬼神双牙斩）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 162 孢子碎击炮 [50] / 遮天光束斩 [200]
    // ════════════════════════════════════════════════════════════════

    // T4 无异常 → 不附加（loss == raw）
    {
        ScenarioBattle b(make_party(self_pet({kBaoZi, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp_before = ctx.getPet(1).hp;
        play_round(b, 0);
        CHECK_EQ(pink_of(ctx, hp_before), 0, "T4 核心：对手无异常 → 不附加");
        std::cout << "  [T4] 162 无异常对照通过\n";
    }

    // T5 对手中毒 → 附加 50（+ 中毒 tick 并入）
    {
        ScenarioBattle b(make_party(self_pet({kBaoZi, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const ApplyAnomalyResult r =
            apply_anomaly(&ctx, 1, kPoisonId, /*duration_rounds=*/3, /*actor=*/0);
        REQUIRE(r == ApplyAnomalyResult::SUCCESS);
        const int hp_before = ctx.getPet(1).hp;
        play_round(b, 0);
        CHECK_EQ(pink_of(ctx, hp_before), 50 + kPoisonTick,
                 "T5 核心：任意异常 → 附加 50（+确定性中毒 tick 1249）");
        std::cout << "  [T5] 162 中毒附加 50 通过\n";
    }

    // T6 第二载体 [200]：中毒 → 附加 200
    {
        ScenarioBattle b(make_party(self_pet({kZheTian, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const ApplyAnomalyResult r =
            apply_anomaly(&ctx, 1, kPoisonId, /*duration_rounds=*/3, /*actor=*/0);
        REQUIRE(r == ApplyAnomalyResult::SUCCESS);
        const int hp_before = ctx.getPet(1).hp;
        play_round(b, 0);
        CHECK_EQ(pink_of(ctx, hp_before), 200 + kPoisonTick, "T6 核心：第二载体 [200] 同判据");
        std::cout << "  [T6] 162 遮天光束斩 200 通过\n";
    }

    std::cout << "scenario_810_ramp_fixed all passed" << std::endl;
    return 0;
}
