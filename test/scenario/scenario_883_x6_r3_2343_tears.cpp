// 场景 883 —— X6/R3 复核收口：2343 循环挡票 × 4泪穿透 双变体（BLOCKED 恒定）
//（2026-10-01，worktree ../agents/x-fix-1；小修批工单 X6 / R 组看板 R3，段 883）
//
// 背景（R 组工单 R3 + 交接档"R3 结案方向"，用户 2026-09-30 定档）：控制台实测 r5/r7
// 的 BLOCKED 经复核属**龙威/无效化盔类**（克雷弗德受击侧两件掉血为 0 机制：叠加减伤
// ——减伤不是挡伤、4泪不可无视；协议·激反龙威——无效化盔 penetrable=false、不可被穿），
// 非 2343 挡票，原"4 泪穿透漏覆盖攻击无效化"系误归因 → 行为正确，待场景收口关单。
//
// 本场景把 R3 交接的三条现成事实逐条落成断言：
//   883a 基线：2343 式挡票（次数型 DAMAGE 免疫票 grant_immunity counts=1，交接事实 1；
//        授予参数对齐 canglan_pink.cpp:197，coverage 放宽为 coverage_all 只影响窗口宽度
//        不影响类别）→ 先手攻击被挡（hp 不动）；票恰好消费一次，第二刀落地。
//   883b ★验收变体（有 4泪）：攻方持 PENETRATE_ATTACK 窗口票（grant_penetration
//        ignore_attack_immunity=true，对齐 2343 ≥4 滴的 grant 调用 canglan_pink.cpp:87）
//        → **BLOCKED 恒定**：管线 BLOCK 回调只查 is_immune(DAMAGE)（battleContext.cpp
//        install_default_damage_block），不问攻方穿透凭证——4泪窗口穿透面不含免伤票/
//        无效化类，与结案方向一致；窗口 rounds=1，R2 过期后照常落地。
//   883c 掩码路径边界（交接事实 3：掩码与窗口票是两条路径，需分别覆盖）：受击方
//        damage_suppress_mask |= BLOCK|DETECT（= 蚀砚之泪≥4滴掩码态，场景 600 T2 同款
//        直写）→ 挡票被抑制、伤害落地——掩码面吃挡伤票（T2 对 794 票已证，此处于
//        2343 类票复证）；两路面合起来仍不含龙威/无效化盔（penetrable=false）。
//
// 结论（写回 R 板 R3 行 + 常识速查）：R3 关单——控制台 BLOCKED 属龙威类，4泪两路面
// （窗口穿透/掩码抑制）均不覆盖它，引擎行为与官方口径一致。
#include <array>
#include <iostream>

#include <effects/damage_pipeline.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kZhuangJi = 10001;   // 撞击：物攻 无效果（纯攻击载体，600 同款）
constexpr int kPaDa = 10004;

const int kSuppressBits = (1 << static_cast<int>(DamageEffectCategory::BLOCK))
                          | (1 << static_cast<int>(DamageEffectCategory::DETECT));

ElfPet self_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 2343 式循环挡票（对齐 canglan_pink.cpp:197：次数型 DAMAGE 免疫票 counts=1）。
void grant_canglan_block_ticket(BattleContext& ctx) {
    ctx.grant_immunity(/*owner=*/1, ImmunityType::DAMAGE, coverage_all(),
                       /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/0,
                       /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/1,
                       /*source_effect_id=*/2343);
}

// 2343 ≥4 滴的穿透窗口（对齐 canglan_pink.cpp:87 grant_penetration 调用）。
void grant_tears_penetration(BattleContext& ctx) {
    ctx.grant_penetration(/*owner=*/0, /*level=*/1, /*ignore_attack_immunity=*/true,
                          /*ignore_damage_limit=*/false, /*counts=*/0, /*rounds=*/1,
                          RuleCategory::PENETRATE_ATTACK, EffectScope::ON_STAGE,
                          /*source_effect_id=*/2343);
}

void play_round_foe_none(ScenarioBattle& b, int self_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // 883a 基线：挡票挡下第一刀、恰好消费一次
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kZhuangJi, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/10, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_canglan_block_ticket(ctx);
        const int hp1 = ctx.getPet(1).hp;
        play_round_foe_none(b, 0);   // R1：先手攻击 → 挡票 BLOCK 阶段消费
        CHECK_EQ(ctx.getPet(1).hp, hp1, "883a 核心：次数型挡票挡下攻击（0 掉血）");
        play_round_foe_none(b, 0);   // R2：票已消费 → 照常落地
        CHECK(ctx.getPet(1).hp < hp1,
              "883a 对照：counts=1 恰好消费一次，第二刀落地（管线与票都是活的）");
        std::cout << "  [883a] 2343 式挡票基线 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 883b ★验收变体：4泪窗口穿透下 BLOCKED 恒定
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kZhuangJi, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/10, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_canglan_block_ticket(ctx);
        grant_tears_penetration(ctx);   // 4泪 ≥4 滴的窗口穿透态
        const int hp1 = ctx.getPet(1).hp;
        play_round_foe_none(b, 0);   // R1：带穿透窗攻击 → 照样被挡
        CHECK_EQ(ctx.getPet(1).hp, hp1,
                 "883b ★核心：4泪 PENETRATE_ATTACK 窗口不穿透免伤票（BLOCKED 恒定——"
                 "穿透面只含可穿攻击免疫/狮盔，不含无效化类）");
        play_round_foe_none(b, 0);   // R2：票消费完 + 窗口过期 → 落地
        CHECK(ctx.getPet(1).hp < hp1, "883b 对照：R2 票尽窗口过 → 照常落地");
        std::cout << "  [883b] 4泪窗口穿透变体（BLOCKED 恒定）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 883c 掩码路径边界：4泪抑制面吃挡票（第二条路径，交接事实 3）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kZhuangJi, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/10, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_canglan_block_ticket(ctx);
        ctx.getPet(1).damage_suppress_mask |= kSuppressBits;   // 蚀砚之泪≥4滴掩码态
        const int hp1 = ctx.getPet(1).hp;
        play_round_foe_none(b, 0);   // R1：掩码抑制挡票 → 伤害落地
        CHECK(ctx.getPet(1).hp < hp1,
              "883c 边界：掩码面（BLOCK|DETECT）抑制挡票、伤害落地（600 T2 同款，"
              "于 2343 类票复证——与 883b 窗口面互为两条独立路径）");
        std::cout << "  [883c] 掩码路径边界 通过\n";
    }

    std::cout << "scenario_883 PASS (X6/R3 收口：2343 挡票 × 4泪双变体 BLOCKED 恒定，段 883)"
              << std::endl;
    return 0;
}
