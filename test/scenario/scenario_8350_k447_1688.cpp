// 场景 8350：K447 · 1688 重盘落地
// ——「消除敌我双方能力提升状态和回合类效果，消除任意一项成功则吸取对手最大体力的
//    1/{0}且{1}%令对手{2}」×5
//（resources/moves_lib/k447_1688_purge_round_drain_anomaly.cpp，2026-11-09；号段
//  8350-8359；K444 1477 已落地基座上的重盘单——diff 与通道见插件头注）
//
// 断言口径（详见插件头注）：单宏落 SKILL_EFFECT 桶，六腿全确定性（概率零裸跑——
// {1}=100% 申报制短路掷点，固定 seed 只防平台抖动）；量值全走审计槽精确断言。
//   T1 门开全清腿（T1-T2 两回合布局——FMR 时点在 SKILL_EFFECT 前，setup 期授予的
//     回合类效果会被本回合 FMR 消费，故 R1 先挂窗 R2 再施放，scenario_1650 同式）：
//     双侧提升（+2/+1，负槽不清）+ 双侧回合类窗（24567 [693] 自身窗 / 28331 [1204]
//     对手窗，均 ContinuousEffect 回合类、可被断——1650 判例）→ 四清全 SUCCESS
//     → 吸取 200000/回血 200000 精确（半血观测——坑集 §4/§9）+ 100% 烧伤落地；
//     has_round_effects 双侧 false 直证窗确实被清。
//   T2 仅回合类开门腿（**1477 差量腿**）：全场无提升、只有双侧回合类窗 → 1477 门
//     必闭、1688 门开（回合类 SUCCESS 计入"任意一项"）→ 吸取/异常照发。
//   T3 双净门闭腿：四清全空 → 门闭 → 无吸取无异常（"则"管辖两段——1468 同句法）；
//     自身血零变化精确；申报槽 13/14 未写（默认哨兵 -99 直证未申报）。
//   T4 {2} 变体腿（29111 [1688,1576][3,100,15]）：{2}=15 冰封逐行参落地（非烧伤病）
//     + 1576 未注册静默跳过（自身血 = 1000+200000 精确——献祭半段确实没跑）。
//   T5 混链腿（27578 [1688,693,843][3,100,2|2,100|2,2]）：1688 链首先跑（消除/吸取/
//     异常），链尾 693/843 同桶共存注册不撞审计。
//   T6 混链+变体腿（38450 [2226,1688][1|3,100,11]）：{2}=11 衰弱；2226 未注册静默
//     跳过；红伤随机浮动只做方向断言（审计槽 exact 免疫红伤方差——SKILL_EFFECT
//     时点 hp 差在 ATTACK_DAMAGE 之前取样）。
#include <algorithm>
#include <any>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa  = 10004;  // 拍打：物40 无效果填充刀（crit_rate=0）
constexpr int k28092  = 28092;  // 净野之焰 [1688]|[3,100,2]：属性技（1688 纯链主载体）
constexpr int k29111  = 29111;  // 凝愫流形 [1688,1576]|[3,100,15,3]：属性技（1576 未注册）
constexpr int k27578  = 27578;  // 以身渡人 [1688,693,843]|[3,100,2,2,100,2,2]：属性技
constexpr int k38450  = 38450;  // 临世圣歌 [2226,1688]|[1,3,100,11]：特85（2226 未注册）
constexpr int k24567  = 24567;  // 虚空灵闪 [693]|[2,100]：属性技（R1 自身回合类窗）
constexpr int k28331  = 28331;  // 灼烈之风 [1204]|[2]：属性技（R1 对手回合类窗，1650 判例）

constexpr int kAudit1688 = 1016880000;

int audit_int(BattleContext& ctx, int owner, int slot, int def = -99) {
    const auto it = ctx.plugin_storage.find(kAudit1688 + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
int a1688(BattleContext& ctx, int o, int s) { return audit_int(ctx, o, s); }

int level(BattleContext& ctx, int side, int slot) { return ctx.ability_levels[side][slot]; }

ElfPet caster(std::array<int, 5> skills, int speed = 300, int hp = 400000, int atk = 1) {
    ElfPet p = scenario::make_pet(0, "施加方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}
ElfPet foe(std::array<int, 5> skills, int speed = 100, int hp = 600000) {
    ElfPet p = scenario::make_pet(1, "对手", skills, speed, /*atk=*/1, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// R1：双方各挂回合类窗（自身 [693] / 对手 [1204]，1650 同式）。
void drive_r1_windows(ScenarioBattle& b, unsigned seed) {
    sbs_srand(seed);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 self: 24567
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 foe:  28331
    b.drive();
}

// R2：我方施放 1688，对手 NONE（不出刀防污染血差断言；NONE=回合类照常）。
void drive_cast(ScenarioBattle& b, unsigned seed) {
    sbs_srand(seed);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 self: 1688 载体
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══════════ T1 门开全清腿（28092：四清全中 + 吸取/回血/异常精确）══════════
    {
        ScenarioBattle b(scenario::make_party(caster({k28092, k24567, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({k28331, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, 0, 2) == StatChangeResult::SUCCESS);   // 自身攻击 +2
        REQUIRE(stat_change(&ctx, 0, 2, -1) == StatChangeResult::SUCCESS);  // 自身防御 -1（负槽不清）
        REQUIRE(stat_change(&ctx, 1, 1, 1) == StatChangeResult::SUCCESS);   // 对手特攻 +1
        drive_r1_windows(b, 20261109);
        CHECK(ctx.has_round_effects(0), "T1 前置：自身回合类窗已挂（24567 R1 后）");
        CHECK(ctx.has_round_effects(1), "T1 前置：对手回合类窗已挂（28331 R1 后）");
        ctx.getPet(0).hp = 1000;   // 压当前体力防回血 clamp（坑集 §4）
        drive_cast(b, 20261109);
        CHECK_EQ(a1688(ctx, 0, 0), 3, "T1 行参 denom=3");
        CHECK_EQ(a1688(ctx, 0, 1), 100, "T1 行参 pct=100");
        CHECK_EQ(a1688(ctx, 0, 2), 2, "T1 行参 {2}=烧伤");
        CHECK_EQ(a1688(ctx, 0, 3), 1, "T1 自身提升清除=1（+2 一项；负槽不清）");
        CHECK_EQ(a1688(ctx, 0, 4), 1, "T1 对手提升清除=1");
        CHECK_EQ(a1688(ctx, 0, 5), 0, "T1 核心：自身断回合=SUCCESS（回合类计入四清）");
        CHECK_EQ(a1688(ctx, 0, 6), 0, "T1 核心：对手断回合=SUCCESS");
        CHECK_EQ(a1688(ctx, 0, 7), 1, "T1 成功门=开");
        CHECK_EQ(a1688(ctx, 0, 8), 1, "T1 使用计数=1");
        CHECK_EQ(a1688(ctx, 0, 9), 0, "T1 无跳过");
        CHECK_EQ(a1688(ctx, 0, 10), 200000, "T1 核心：对手实扣=600000/3=200000");
        CHECK_EQ(a1688(ctx, 0, 11), 200000, "T1 核心：回血实绩=200000");
        CHECK_EQ(a1688(ctx, 0, 12), 0, "T1 drain 返回=SUCCESS");
        CHECK_EQ(a1688(ctx, 0, 13), 100, "T1 申报概率={1}=100（掷前申报）");
        CHECK_EQ(a1688(ctx, 0, 14), 0, "T1 异常落地=SUCCESS");
        CHECK_EQ(level(ctx, 0, 0), 0, "T1 自身攻击槽清零");
        CHECK_EQ(level(ctx, 0, 2), -1, "T1 自身防御负槽保留（只清提升，1477 同判）");
        CHECK_EQ(level(ctx, 1, 1), 0, "T1 对手特攻槽清零");
        CHECK(!ctx.has_round_effects(0), "T1 核心：自身回合类窗确实被清");
        CHECK(!ctx.has_round_effects(1), "T1 核心：对手回合类窗确实被清");
        CHECK(ctx.has_active_abnormal_status(1, 2), "T1 核心：100% 烧伤落地");
        CHECK_EQ(ctx.getPet(0).hp, 201000, "T1 核心：自身 1000+200000=201000");
        CHECK_EQ(ctx.getPet(1).hp, 325000,
                 "T1 核心：对手 600000-200000(吸取)-75000(烧伤本回合扣 1/8，坑集 §5 同源)=325000");
        std::cout << "  [T1] 1688 门开全清腿（四清 + 200000 吸取/回血 + 烧伤）通过\n";
    }

    // ══════════ T2 仅回合类开门腿（1477 差量腿：无提升、只有回合类窗）══════════
    {
        ScenarioBattle b(scenario::make_party(caster({k28092, k24567, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({k28331, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(level(ctx, 0, 0), 0, "T2 前置：自身无提升");
        CHECK_EQ(level(ctx, 1, 0), 0, "T2 前置：对手无提升");
        drive_r1_windows(b, 20261109);
        ctx.getPet(0).hp = 1000;
        drive_cast(b, 20261109);
        CHECK_EQ(a1688(ctx, 0, 3), 0, "T2 自身提升清除=0");
        CHECK_EQ(a1688(ctx, 0, 4), 0, "T2 对手提升清除=0");
        CHECK_EQ(a1688(ctx, 0, 5), 0, "T2 自身断回合=SUCCESS");
        CHECK_EQ(a1688(ctx, 0, 6), 0, "T2 核心：对手断回合=SUCCESS");
        CHECK_EQ(a1688(ctx, 0, 7), 1, "T2 核心：门开（回合类 SUCCESS 计入任意一项——1688 对 1477 差量）");
        CHECK_EQ(a1688(ctx, 0, 10), 200000, "T2 核心：吸取 200000 照发");
        CHECK_EQ(a1688(ctx, 0, 11), 200000, "T2 回血 200000");
        CHECK(ctx.has_active_abnormal_status(1, 2), "T2 核心：异常照发（同一则管辖）");
        CHECK_EQ(ctx.getPet(0).hp, 201000, "T2 自身 201000");
        CHECK_EQ(ctx.getPet(1).hp, 325000, "T2 对手 325000（吸取 200000 + 烧伤 tick 75000）");
        std::cout << "  [T2] 1688 仅回合类开门腿（1477 差量）通过\n";
    }

    // ══════════ T3 双净门闭腿（四清全空 → 无吸取无异常）══════════
    {
        ScenarioBattle b(scenario::make_party(caster({k28092, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int self0 = ctx.getPet(0).hp;
        sbs_srand(20261109);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(a1688(ctx, 0, 3), 0, "T3 自身清除=0");
        CHECK_EQ(a1688(ctx, 0, 4), 0, "T3 对手清除=0");
        CHECK_EQ(a1688(ctx, 0, 5), 1, "T3 自身断回合=NO_EFFECTS");
        CHECK_EQ(a1688(ctx, 0, 6), 1, "T3 对手断回合=NO_EFFECTS");
        CHECK_EQ(a1688(ctx, 0, 7), 0, "T3 核心：成功门=四源全空");
        CHECK_EQ(a1688(ctx, 0, 9), 3, "T3 跳过原因=3（门闭）");
        CHECK_EQ(a1688(ctx, 0, 10), 0, "T3 对手实扣=0（无吸取）");
        CHECK_EQ(a1688(ctx, 0, 11), 0, "T3 回血=0");
        CHECK_EQ(a1688(ctx, 0, 13), -99, "T3 核心：申报槽未写（门闭未申报——则管辖两段）");
        CHECK_EQ(a1688(ctx, 0, 14), -99, "T3 核心：落地槽未写");
        CHECK(!ctx.has_active_abnormal_status(1, 2), "T3 核心：无异常附加");
        CHECK_EQ(ctx.getPet(0).hp, self0, "T3 核心：自身血零变化（属性技+无吸取）");
        CHECK_EQ(ctx.getPet(1).hp, 600000, "T3 对手血零变化");
        std::cout << "  [T3] 1688 双净门闭腿通过\n";
    }

    // ══════════ T4 {2} 变体腿（29111：冰封 15 + 1576 未注册静默）══════════
    {
        ScenarioBattle b(scenario::make_party(caster({k29111, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, 0, 1) == StatChangeResult::SUCCESS);  // 对手攻击 +1 开门
        ctx.getPet(0).hp = 1000;
        sbs_srand(20261110);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(a1688(ctx, 0, 2), 15, "T4 核心：行参 {2}=15（冰封逐载体消费）");
        CHECK_EQ(a1688(ctx, 0, 4), 1, "T4 对手清除=1（门经提升源开）");
        CHECK_EQ(a1688(ctx, 0, 10), 200000, "T4 吸取 200000 精确");
        CHECK(ctx.has_active_abnormal_status(1, 15), "T4 核心：冰封落地");
        CHECK(!ctx.has_active_abnormal_status(1, 2), "T4 核心：非烧伤（{2} 直传锁定）");
        CHECK_EQ(ctx.getPet(0).hp, 201000, "T4 核心：自身 201000（1576 未注册献祭半段确实没跑）");
        std::cout << "  [T4] 1688 {2} 变体腿（29111 冰封）通过\n";
    }

    // ══════════ T5 混链腿（27578：链首 1688 + 链尾 693/843 同桶共存）══════════
    {
        ScenarioBattle b(scenario::make_party(caster({k27578, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, 4, 1) == StatChangeResult::SUCCESS);  // 对手速度 +1 开门
        ctx.getPet(0).hp = 1000;
        sbs_srand(20261111);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(a1688(ctx, 0, 3), 0, "T5 自身清除=0");
        CHECK_EQ(a1688(ctx, 0, 4), 1, "T5 对手清除=1");
        CHECK_EQ(a1688(ctx, 0, 6), 1, "T5 对手断回合=NO_EFFECTS（单回合无窗）");
        CHECK_EQ(a1688(ctx, 0, 7), 1, "T5 门开（提升源）");
        CHECK_EQ(a1688(ctx, 0, 10), 200000, "T5 核心：混链下吸取 200000 精确");
        CHECK(ctx.has_active_abnormal_status(1, 2), "T5 烧伤落地");
        CHECK_EQ(ctx.getPet(0).hp, 201000, "T5 回血=200000");
        CHECK_EQ(ctx.getPet(1).hp, 325000, "T5 对手 325000（吸取 200000 + 烧伤 tick 75000）");
        std::cout << "  [T5] 1688 混链腿（27578 链首 1688）通过\n";
    }

    // ══════════ T6 混链+变体腿（38450：{2}=11 衰弱 + 2226 未注册静默）══════════
    {
        ScenarioBattle b(scenario::make_party(caster({k38450, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, 3, 1) == StatChangeResult::SUCCESS);  // 对手特防 +1 开门
        ctx.getPet(0).hp = 1000;
        sbs_srand(20261112);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(a1688(ctx, 0, 2), 11, "T6 核心：行参 {2}=11（衰弱）");
        CHECK_EQ(a1688(ctx, 0, 10), 200000, "T6 核心：吸取 200000 精确（SKILL_EFFECT 时点 hp 差，免疫红伤方差）");
        CHECK_EQ(a1688(ctx, 0, 14), 0, "T6 异常落地=SUCCESS");
        CHECK(ctx.has_active_abnormal_status(1, 11), "T6 核心：衰弱落地");
        CHECK_EQ(ctx.getPet(0).hp, 201000, "T6 回血=200000（对手 NONE 无红伤污染）");
        CHECK(ctx.getPet(1).hp < 600000, "T6 对手掉血（吸取+红伤，方向断言）");
        std::cout << "  [T6] 1688 混链+变体腿（38450 衰弱）通过\n";
    }

    std::cout << "scenario_8350: ALL PASS" << std::endl;
    return 0;
}
