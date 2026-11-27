// 场景 8210：K434 必杀双宏族 —— 1325「对手不处于能力下降状态则必定打出致命一击」+
// 1748「本回合若未打出致命一击则下{0}回合自身攻击必定致命一击」
//（resources/moves_lib/k434_bisha_pool.cpp，2026-11-08；号段 8210-8219）
//
// 覆盖（1325 判据 = 对手六槽无任一负等级；必暴通道 = SKILL_EFFECT 直写
//       crit_happened——暴击掷点在 query_usage ①.5 早于 SKILL_EFFECT，
//       ATTACK_DAMAGE 只消费旗标 ×2；1748 = crit_happened 门 + NextRounds
//       ROUND_START tick 写 ws.must_crit，1204 同通道）：
// ── 1325（审计段 1013250000）──
//   T1  37885 铭文低吟 ×2 闭式对照：T1a 对手预置物攻-1（门闭）→ D1；T1b 干净盘
//       （门开）同 seed → D2 == 2×D1（同 draw 序、暴击系数 200 精确放大）；
//       审计 slot0..3 全申报 + crit_happened/isCrit 旗
//   T2  边界：T2a 对手 +2 双正槽（门仍开——只看下降面）；T2b -2 双负槽 slot3=2 /
//       T2c 混合 -1+3 slot3=1 门闭（正槽不抵消负槽）
//   T3  双方镜像（双方 37885 干净盘互打）：两侧审计各自 slot2=1（owner×16 隔离）、
//       双侧 crit_happened/isCrit 真置
//   T4  [1243,1953,449,1325] 36866 最长链零串位：3 兄弟（空盘全 no-op：1953 门=
//       对手有降态 / 449 门=对手有降态）不污染 → 1325 照常门开强制
// ── 1748（审计段 1017480000）──
//   V1  37921 [1]（crit_rate=0）：R0 门开开窗（slot2=1 NextRounds 本回合不算、
//       R0 自身未暴）；R1 拍打 → tick 写 must_crit → 必暴（D == 2×V2 的 D1 闭式）；
//       R2 窗过期 slot4 钉 1（rounds=1 直证）
//   V2  同载体负例（set_crit_sixteenths(16) R0 必暴 → 门闭）：slot1=1 slot2=0
//       slot5=1 无窗；R1 拍打 slot4=0 无暴（D1 供 V1 对照）
//   V3  双方镜像 ×rounds=2（35553 [553,1748]|[2]，553 门=暴击 R0 不触发）：R0 双开窗
//       owner×16 隔离；R1/R2 双侧 tick+必暴（slot4=1→2）；R3 双窗过期 slot4 钉 2
//   V4  [1164,1748,1722] 36351 白金碎星弹混链激活（K430 头注"1748 未实现保持
//       no-op"销账直证）：R0 双审计段齐写（1748 slot2=1 + K430 1722 slot3=1/
//       slot4=1，切片 [1, 1, 1,3] 参不串位）；R1 拍打必暴（1748 tick）+
//       1722 先制窗共存
//
// 确定性纪律：掷点敏感断言全部闭式核验（MINSTD r_k=(seed×16807^k mod 2^31-1)%100；
//   本两宏零掷点——1325 门判据读槽、置旗；1748 tick 置旗——暴击消费面
//  （battleFsm.cpp:716-718）不重掷，伤害方差每结算恰 1 抽
//  （calculation.h:87 ×(217+rand%39)/255），同 seed 双局 draw 序对齐 → ×2 精确）。
// 抽取账本：must_hit 全置 1（零命中掷点）、crit_rate=0（harness 缺省，①.5 短路零抽）、
//   速度 300/100 无先手掷点 → 每回合抽取 = 伤害方差 1 抽/侧。V2 的 R0 必暴走
//   rate>=100 短路（skills.cpp:964 ||链）同样零抽 → V1/V2 R0 抽取数一致。
#include <array>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <effects/rule_center.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <fsm/state.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kMingWen  = 37885;   // 铭文低吟   [1325]|[0]：物140（唯一纯独载体）
constexpr int kDaMo     = 36866;   // 大漠狂沙烫 [1243,1953,449,1325]|[0]：物150（最长链）
constexpr int kWanJie   = 37921;   // 天崩万界破 [485,1748]|[1]：物160
constexpr int kZhiBi    = 35553;   // 执笔绘春秋 [553,1748]|[2]：特150
constexpr int kBaiJin   = 36351;   // 白金碎星弹 [1164,1748,1722]|[1, 1, 1,3]：物160（混链）
constexpr int kPaiDa    = 10004;   // 拍打：物40 干净填充刀（窗内攻击技）

constexpr int kAudit1325 = 1013250000;
constexpr int kAudit1748 = 1017480000;
constexpr int kAudit1722 = 1017220000;   // K430 1722 审计段（V4 混链销账对照）

int a1325(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAudit1325 + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
int a1748(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAudit1748 + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
int a1722(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAudit1722 + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

constexpr int kMyMax = 400000;
constexpr int kFoeMax = 600000;

ElfPet pet_with_elem(int side, const char* name, std::array<int, 5> skills, int speed,
                     int atk, int hp) {
    ElfPet p = scenario::make_pet(side, name, skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet caster(std::array<int, 5> skills) {
    return pet_with_elem(0, "施加方", skills, /*speed=*/300, /*atk=*/300, kMyMax);
}

ElfPet foe(std::array<int, 5> skills, int atk = 1) {
    return pet_with_elem(1, "对手", skills, /*speed=*/100, atk, kFoeMax);
}

// 单回合驱动：0 侧出指定技能、对手静默（不耗抽取）。
void cast_round(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 对手侧行动回合（镜像轮）。
void foe_attack_round(ScenarioBattle& b, int foe_skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_skill_index);
    b.drive();
}

// 双方互打回合（T3/T6 镜像）。
void both_attack_round(ScenarioBattle& b, int s0_idx, int s1_idx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, s0_idx);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, s1_idx);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 1325 ×2 闭式对照：门闭 D1 / 门开 D2==2×D1（同 seed）==========
    {
        constexpr int kSeed = 20261108;
        {   // T1a 门闭对照：对手预置物攻 -1（槽 0；不碰防御槽 → 伤害基不变）
            ScenarioBattle b(scenario::make_party(caster({kMingWen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.ability_levels[1][0] = -1;   // 对手处于能力下降状态（开战清零后预置）
            b.ctx().seed_rng(kSeed);
            const int before = ctx.getPet(1).hp;
            cast_round(b, 0);
            const int d1 = before - ctx.getPet(1).hp;
            CHECK_EQ(a1325(ctx, 0, 0), 0, "T1a 条件观测=0（对手有负槽 门闭）");
            CHECK_EQ(a1325(ctx, 0, 1), 0, "T1a 置暴=0");
            CHECK_EQ(a1325(ctx, 0, 2), 0, "T1a 置暴计数=0");
            CHECK_EQ(a1325(ctx, 0, 3), 1, "T1a 负槽快照=1");
            CHECK(ctx.crit_happened[0] == false, "T1a 未强制暴击");
            CHECK(ctx.resolvedDamage.isCrit == false, "T1a isCrit=否");
            CHECK(d1 > 0, "T1a 物140 落伤（对照可用）");
            std::cout << "  [T1a] 1325 门闭对照 D1=" << d1 << " 通过\n";

            // T1b 门开：同 seed 同布局、对手干净 → 强制暴击 D2 == 2×D1
            ScenarioBattle b2(scenario::make_party(caster({kMingWen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b2.start();
            BattleContext& ctx2 = b2.ctx();
            b2.ctx().seed_rng(kSeed);   // 同 seed → 伤害方差同抽（每结算恰 1 抽）
            const int before2 = ctx2.getPet(1).hp;
            cast_round(b2, 0);
            const int d2 = before2 - ctx2.getPet(1).hp;
            CHECK_EQ(a1325(ctx2, 0, 0), 1, "T1b 条件观测=1（对手无负槽 门开）");
            CHECK_EQ(a1325(ctx2, 0, 1), 1, "T1b 置暴=1");
            CHECK_EQ(a1325(ctx2, 0, 2), 1, "T1b 置暴计数=1");
            CHECK_EQ(a1325(ctx2, 0, 3), 0, "T1b 负槽快照=0");
            CHECK(ctx2.crit_happened[0] == true, "T1b crit_happened 直写真置");
            CHECK(ctx2.resolvedDamage.isCrit == true, "T1b isCrit=是（ATTACK_DAMAGE 消费）");
            CHECK_EQ(d2, 2 * d1, "T1b 暴击 ×2 闭式（D2==2×D1，同方差抽 + 系数 200；实际 D2=" +
                                     std::to_string(d2) + " 2×D1=" + std::to_string(2 * d1) + "）");
            std::cout << "  [T1b] 1325 门开强制 D2=" << d2 << "=2×D1 通过\n";
        }
    }

    //========== T2 1325 边界：正槽不闭门 / 多负槽 / 混合槽 ==========
    {
        {   // T2a 对手 +2 双正槽（防御+特防）：门仍开（模板只看下降面）
            ScenarioBattle b(scenario::make_party(caster({kMingWen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.ability_levels[1][2] = 2;
            ctx.ability_levels[1][3] = 2;
            b.ctx().seed_rng(20261109);
            cast_round(b, 0);
            CHECK_EQ(a1325(ctx, 0, 0), 1, "T2a 正槽不闭门（观测=1）");
            CHECK_EQ(a1325(ctx, 0, 3), 0, "T2a 负槽快照=0");
            CHECK(ctx.crit_happened[0] == true, "T2a 照常强制暴击");
            std::cout << "  [T2a] 1325 正槽边界 通过\n";
        }
        {   // T2b 对手 -2 双负槽：slot3=2 门闭
            ScenarioBattle b(scenario::make_party(caster({kMingWen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.ability_levels[1][0] = -2;
            ctx.ability_levels[1][4] = -2;
            b.ctx().seed_rng(20261110);
            cast_round(b, 0);
            CHECK_EQ(a1325(ctx, 0, 3), 2, "T2b 负槽快照=2（负槽计数与降幅无关）");
            CHECK_EQ(a1325(ctx, 0, 0), 0, "T2b 门闭");
            CHECK(ctx.crit_happened[0] == false, "T2b 未强制");
            std::cout << "  [T2b] 1325 双负槽边界 通过\n";
        }
        {   // T2c 混合 -1（物攻）+3（速度）：正槽不抵消 → 门闭 slot3=1
            ScenarioBattle b(scenario::make_party(caster({kMingWen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.ability_levels[1][0] = -1;
            ctx.ability_levels[1][4] = 3;
            b.ctx().seed_rng(20261111);
            cast_round(b, 0);
            CHECK_EQ(a1325(ctx, 0, 3), 1, "T2c 负槽快照=1（正槽不计入）");
            CHECK_EQ(a1325(ctx, 0, 0), 0, "T2c 门闭（正槽不抵消负槽）");
            CHECK(ctx.crit_happened[0] == false, "T2c 未强制");
            std::cout << "  [T2c] 1325 混合槽边界 通过\n";
        }
    }

    //========== T3 1325 双方镜像：互打互强制，键段 owner×16 隔离 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kMingWen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kMingWen, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*atk=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.ctx().seed_rng(20261112);
        const int before0 = ctx.getPet(0).hp;
        const int before1 = ctx.getPet(1).hp;
        both_attack_round(b, 0, 0);
        CHECK_EQ(a1325(ctx, 0, 0), 1, "T3 0 侧观测=1");
        CHECK_EQ(a1325(ctx, 1, 0), 1, "T3 1 侧观测=1（镜像同判据）");
        CHECK_EQ(a1325(ctx, 0, 2), 1, "T3 0 侧置暴计数=1");
        CHECK_EQ(a1325(ctx, 1, 2), 1, "T3 1 侧置暴计数=1（键段 owner×16 隔离）");
        CHECK_EQ(a1325(ctx, 0, 3), 0, "T3 0 侧负槽快照=0");
        CHECK_EQ(a1325(ctx, 1, 3), 0, "T3 1 侧负槽快照=0");
        CHECK(ctx.crit_happened[0] == true, "T3 0 侧强制暴击");
        CHECK(ctx.crit_happened[1] == true, "T3 1 侧强制暴击（互打互置）");
        CHECK(before1 - ctx.getPet(1).hp > 0, "T3 0 侧落伤");
        CHECK(before0 - ctx.getPet(0).hp > 0, "T3 1 侧落伤");
        std::cout << "  [T3] 1325 双方镜像+键段隔离 通过\n";
    }

    //========== T4 1325 最长链 [1243,1953,449,1325] 零串位 ==========
    {
        // 链兄弟空盘全 no-op：1243 对手槽优先（无前置）/ 1953 门=对手有降态 /
        // 449 门=对手有降态 → 1325 零参切片不受 3 段兄弟参污染。
        ScenarioBattle b(scenario::make_party(caster({kDaMo, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.ctx().seed_rng(20261113);
        cast_round(b, 0);
        CHECK_EQ(a1325(ctx, 0, 0), 1, "T4 门开（长链尾零参切片正确）");
        CHECK_EQ(a1325(ctx, 0, 1), 1, "T4 置暴=1");
        CHECK_EQ(a1325(ctx, 0, 3), 0, "T4 负槽快照=0");
        CHECK(ctx.crit_happened[0] == true, "T4 强制暴击落地");
        CHECK(ctx.resolvedDamage.isCrit == true, "T4 isCrit 消费");
        std::cout << "  [T4] 1325 最长链零串位 通过\n";
    }

    //========== V2 1748 负例：R0 自身必暴 → 门闭无窗（D1 供 V1 闭式对照）==========
    {
        // crit_rate=16/16 → ①.5 rate>=100 短路必暴（零抽取）→ SKILL_EFFECT 旗已真
        // → 门闭。⚠️ 只设载体槽（槽 0）——set_crit_sixteenths 全技能设满会让 R1 拍打
        // 也必暴（首跑实测 289 行红）。485 链兄弟（消强成功→回满血）对无强化对手 no-op。
        ElfPet crit_caster = caster({kWanJie, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
        crit_caster.skills[0].critical_strike_rate = 100.0f;   // 16/16 官方分子折算
        ScenarioBattle b(scenario::make_party(crit_caster),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.ctx().seed_rng(20261114);
        cast_round(b, 0);   // R0 必暴载体
        CHECK_EQ(a1748(ctx, 0, 0), 1, "V2 申报回合数=1");
        CHECK_EQ(a1748(ctx, 0, 1), 1, "V2 暴击旗观测=1（本回合已暴击）");
        CHECK_EQ(a1748(ctx, 0, 2), 0, "V2 窗未注册（start=0）");
        CHECK_EQ(a1748(ctx, 0, 3), 0, "V2 开窗计数=0");
        CHECK_EQ(a1748(ctx, 0, 4), 0, "V2 tick=0");
        CHECK_EQ(a1748(ctx, 0, 5), 1, "V2 拒绝原因=1（本回合已暴击门闭）");
        const int before = ctx.getPet(1).hp;
        cast_round(b, 1);   // R1 拍打：无窗无 tick
        const int d1 = before - ctx.getPet(1).hp;
        CHECK_EQ(a1748(ctx, 0, 4), 0, "V2 R1 tick 钉 0（无窗）");
        CHECK(ctx.crit_happened[0] == false, "V2 R1 拍打未暴（crit_rate=0 且无 must_crit）");
        CHECK(ctx.resolvedDamage.isCrit == false, "V2 R1 isCrit=否");
        CHECK(d1 > 0, "V2 R1 拍打落伤（对照可用）");
        std::cout << "  [V2] 1748 门闭负例 D1=" << d1 << " 通过\n";

        //========== V1 1748 正例 rounds=1：R0 开窗（本回合不算）→ R1 必暴 ×D1 → R2 过期 ==
        ScenarioBattle b2(scenario::make_party(caster({kWanJie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b2.start();
        BattleContext& ctx2 = b2.ctx();
        b2.ctx().seed_rng(20261114);   // 同 seed → V1/V2 每回合方差抽序对齐
        cast_round(b2, 0);   // R0 载体（crit_rate=0 未暴 → 门开）
        CHECK_EQ(a1748(ctx2, 0, 0), 1, "V1 申报回合数=1");
        CHECK_EQ(a1748(ctx2, 0, 1), 0, "V1 暴击旗观测=0");
        CHECK_EQ(a1748(ctx2, 0, 2), 1, "V1 窗起点=1（NextRounds 本回合不算）");
        CHECK_EQ(a1748(ctx2, 0, 3), 1, "V1 开窗计数=1");
        CHECK_EQ(a1748(ctx2, 0, 5), 0, "V1 拒绝原因=0");
        CHECK(ctx2.crit_happened[0] == false, "V1 R0 自身未暴（窗当回合不生效）");
        const int before2 = ctx2.getPet(1).hp;
        cast_round(b2, 1);   // R1 拍打 → ROUND_START tick 写 must_crit → 必暴
        const int d2 = before2 - ctx2.getPet(1).hp;
        CHECK_EQ(a1748(ctx2, 0, 4), 1, "V1 R1 tick=1");
        CHECK(ctx2.crit_happened[0] == true, "V1 R1 must_crit 短路必暴");
        CHECK(ctx2.resolvedDamage.isCrit == true, "V1 R1 isCrit=是");
        CHECK_EQ(d2, 2 * d1, "V1 R1 暴击 ×2 闭式（D2==2×D1 同方差抽；实际 D2=" +
                                 std::to_string(d2) + " 2×D1=" + std::to_string(2 * d1) + "）");
        const int before3 = ctx2.getPet(1).hp;
        cast_round(b2, 1);   // R2 拍打 → 窗已过期（rounds=1）
        CHECK_EQ(a1748(ctx2, 0, 4), 1, "V1 R2 tick 钉 1（rounds=1 窗过期——时长 {0} 直证）");
        CHECK(ctx2.crit_happened[0] == false, "V1 R2 未暴（窗外正常掷、crit_rate=0 不暴）");
        CHECK(before3 - ctx2.getPet(1).hp > 0, "V1 R2 照常落伤");
        std::cout << "  [V1] 1748 rounds=1 开窗-必暴-过期 通过\n";
    }

    //========== V3 1748 双方镜像 ×rounds=2（35553 [2]）：R1/R2 双暴、R3 过期 ==========
    {
        // 553 链兄弟（门=暴击施加异常）R0 未暴不触发；特150 载体零红伤方差面同账。
        ScenarioBattle b(scenario::make_party(caster({kZhiBi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kZhiBi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*atk=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.ctx().seed_rng(20261115);
        both_attack_round(b, 0, 0);   // R0 双载体开窗
        CHECK_EQ(a1748(ctx, 0, 0), 2, "V3 0 侧回合数=2（35553 切片）");
        CHECK_EQ(a1748(ctx, 1, 0), 2, "V3 1 侧回合数=2（参不串位）");
        CHECK_EQ(a1748(ctx, 0, 2), 1, "V3 0 侧窗起点=1");
        CHECK_EQ(a1748(ctx, 1, 2), 1, "V3 1 侧窗起点=1（键段隔离）");
        CHECK_EQ(a1748(ctx, 0, 3), 1, "V3 0 侧开窗计数=1");
        CHECK_EQ(a1748(ctx, 1, 3), 1, "V3 1 侧开窗计数=1");
        both_attack_round(b, 1, 1);   // R1 双拍打 → 双 tick 双必暴
        CHECK_EQ(a1748(ctx, 0, 4), 1, "V3 R1 0 侧 tick=1");
        CHECK_EQ(a1748(ctx, 1, 4), 1, "V3 R1 1 侧 tick=1");
        CHECK(ctx.crit_happened[0] == true, "V3 R1 0 侧必暴");
        CHECK(ctx.crit_happened[1] == true, "V3 R1 1 侧必暴");
        both_attack_round(b, 1, 1);   // R2 双拍打 → 窗第二跳（rounds=2：R1/R2）
        CHECK_EQ(a1748(ctx, 0, 4), 2, "V3 R2 0 侧 tick=2（rounds=2 直证）");
        CHECK_EQ(a1748(ctx, 1, 4), 2, "V3 R2 1 侧 tick=2");
        CHECK(ctx.crit_happened[0] == true, "V3 R2 0 侧必暴");
        CHECK(ctx.crit_happened[1] == true, "V3 R2 1 侧必暴");
        both_attack_round(b, 1, 1);   // R3 双拍打 → 双窗过期
        CHECK_EQ(a1748(ctx, 0, 4), 2, "V3 R3 0 侧 tick 钉 2（窗过期）");
        CHECK_EQ(a1748(ctx, 1, 4), 2, "V3 R3 1 侧 tick 钉 2");
        CHECK(ctx.crit_happened[0] == false, "V3 R3 0 侧未暴");
        CHECK(ctx.crit_happened[1] == false, "V3 R3 1 侧未暴");
        std::cout << "  [V3] 1748 双方镜像 rounds=2（R1/R2 双暴、R3 过期）通过\n";
    }

    //========== V4 36351 白金碎星弹混链激活：1748+1722 双审计段齐写（K430 销账）==========
    {
        // K430 头注记档"36351 混链（1748 未实现、保持 no-op）"——本单注册后三链
        // （1164+1748+1722）全在册。切片 [1164]|[1] + 1748|[1] + 1722|[1,3]：
        //   1748 {0}=1 开必暴窗；1722 {0}=1/{1}=3 开先制窗（未暴击兜底）。
        // 1164（迴还：反转或消除对手提升）对无强化对手 no-op 不吃抽取。
        ScenarioBattle b(scenario::make_party(caster({kBaiJin, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.ctx().seed_rng(20261116);
        cast_round(b, 0);   // R0 载体（crit_rate=0 未暴 → 双窗齐开）
        CHECK_EQ(a1748(ctx, 0, 0), 1, "V4 1748 申报回合数=1");
        CHECK_EQ(a1748(ctx, 0, 1), 0, "V4 1748 暴击旗观测=0");
        CHECK_EQ(a1748(ctx, 0, 2), 1, "V4 1748 窗起点=1（混链激活直证）");
        CHECK_EQ(a1748(ctx, 0, 3), 1, "V4 1748 开窗计数=1");
        CHECK_EQ(a1722(ctx, 0, 0), 1, "V4 1722 申报回合数=1（切片 {0}=1）");
        CHECK_EQ(a1722(ctx, 0, 1), 3, "V4 1722 申报先制=3（切片 {1}=3 参不串位）");
        CHECK_EQ(a1722(ctx, 0, 2), 0, "V4 1722 暴击旗观测=0");
        CHECK_EQ(a1722(ctx, 0, 3), 1, "V4 1722 窗起点=1（同判据同开）");
        CHECK_EQ(a1722(ctx, 0, 4), 1, "V4 1722 开窗计数=1");
        cast_round(b, 1);   // R1 拍打 → 1748 tick 必暴；1722 先制窗同跳（先制+3）
        CHECK_EQ(a1748(ctx, 0, 4), 1, "V4 R1 1748 tick=1");
        CHECK(ctx.crit_happened[0] == true, "V4 R1 必暴（1748 tick 落地）");
        CHECK(ctx.resolvedDamage.isCrit == true, "V4 R1 isCrit=是");
        std::cout << "  [V4] 36351 混链激活（1748+1722 双窗共存）K430 no-op 销账直证 通过\n";
    }

    std::cout << "scenario_8210 PASS (K434 必杀双宏族 1325/1748)" << std::endl;
    return 0;
}
