// 场景 8270：K439 K438 留档候选直接落地五宏 —— 1498「随机附加烧伤，冻伤，失明，失神
// 中的{0}种异常状态，未触发则自身下{1}回合造成的伤害提升{2}%」×5 + 1134「将自身能力
// 下降状态反馈给对手，反馈失败则令自身全属性+{0}」×6 + 1359「…反馈成功则自身全属性
// +{0}，反馈失败则解除自身能力下降状态」×6 + 1771「反转对手能力提升状态，反转成功则
// {0}回合内对手的能力下降状态无法被解除或反转」×6 + 1146「双方每处于{0}种能力提升状态
// 则附加{1}点固定伤害」×7
//（resources/moves_lib/k439_feedback_amp_fixed.cpp，2026-11-09；号段 8270-8279 一文件
//  覆盖五族，scenario_8260 同式）
//
// 断言口径（详见插件头注）：五宏全落 SKILL_EFFECT 桶。
//   1498 通道 = attach_random_anomalies（K438 1116/1624/2074 直系同形）+ 776/827
//     NextRounds AMP 窗（ws.damage_add_pct 只由红伤管线消费；字段断言不走伤害比例——
//     坑集 §6 同理）。随机面固定两腿：池改写票缩池（084/8250/8260 同款）+ 对手 Modern
//     异常免疫造 landed==0 补偿路；命名池边界用**恒等观测票**种子无关直证。
//   1134/1359 通道 = 63/1876 反馈拼装（快照→镜像→清自身，SUCCESS 才算落地 2026-10-25
//     口径）+ all_stats_up（1001 同式六槽）。AT_FLOOR/被锁/无可反馈三失败源各一腿，
//     逐槽快照断言（坑集 §1：不看总和）。
//   1771 通道 = stat_boost_reversal（970 快照同式）+ STAT_DROP_LOCK 票（1684/d5_D20:187
//     字面同款，票挂对手）。REVERSED/NOTHING/BLOCKED 三档各一腿 + 功能直证
//     （clear_stat_drops 返 0 / stat_reversal 返 BLOCKED）。
//   1146 通道 = fixed_damage（1154 同通道）。计数面用**提升态种数差分**同 seed 双局
//     （差值恰 = {1}）+ 审计槽绝对值断言；观测槽选 速/命中（不入伤害公式，不翻先手）。
// 断言种子无关，无随机裸跑；对手侧一律无还手（NONE），我方技能 must_hit=1。
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

constexpr int kXinghan  = 36515;  // 星汉璀璨 [1498][2,1,100]：特130（1498 纯独载体）
constexpr int kShenying = 36679;  // 深影之念 [1498][2,1,50]：特140（1498 pct 变体）
constexpr int kGuangying= 33471;  // 光影天闪 [1134][1]：物150 必中（1134 纯独载体）
constexpr int kLingjian = 32336;  // 灵剑封魂 [1359,795][1,25,100]：物85 必中（1359 链，795=当回合增伤尾）
constexpr int kJingye   = 35660;  // 静夜沉沉 [1771,1478][2,1]：特85（1771 [2] 载体，1478=封切换尾）
constexpr int kXiuluo   = 38493;  // 修罗百破击 [2554,881,835,1771,1821][3,5]：物150 必中（1771 [3] 切片载体）
constexpr int kHuaguang = 33108;  // 华光冲击 [1146][1,50]：物130（1146 纯独载体）
constexpr int kHuangsha = 33316;  // 黄沙风暴 [1146][1,40]：特140（1146 amount 变体）
constexpr int kPaiDa    = 10004;  // 拍打：物40 无效果填充刀

constexpr int kAudit1498 = 1014980000;   // K439 1498 键段
constexpr int kAudit1134 = 1011340000;   // K439 1134 键段
constexpr int kAudit1359 = 1013590000;   // K439 1359 键段
constexpr int kAudit1771 = 1017710000;   // K439 1771 键段
constexpr int kAudit1146 = 1011460000;   // K439 1146 键段
constexpr int kPoolObs   = 2092498000;   // T2 恒等观测票落点（场景私有 2.092498B 段）

int audit_int(BattleContext& ctx, int base, int owner, int slot, int def = -99) {
    const auto it = ctx.plugin_storage.find(base + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
int a1498(BattleContext& ctx, int owner, int slot) { return audit_int(ctx, kAudit1498, owner, slot); }
int a1134(BattleContext& ctx, int owner, int slot) { return audit_int(ctx, kAudit1134, owner, slot); }
int a1359(BattleContext& ctx, int owner, int slot) { return audit_int(ctx, kAudit1359, owner, slot); }
int a1771(BattleContext& ctx, int owner, int slot) { return audit_int(ctx, kAudit1771, owner, slot); }
int a1146(BattleContext& ctx, int owner, int slot) { return audit_int(ctx, kAudit1146, owner, slot); }

int active_anomaly_count(BattleContext& ctx, int side) {
    int n = 0;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) ++n;
    }
    return n;
}

ElfPet caster(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = scenario::make_pet(0, "施加方", skills, speed, /*atk=*/300, 400000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}
ElfPet foe(std::array<int, 5> skills, int speed = 100) {
    ElfPet p = scenario::make_pet(1, "对手", skills, speed, /*atk=*/1, 600000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// R1 骨架：我方出载体，对手无还手。
void run_cast(ScenarioBattle& b, int skill_index, int seed) {
    b.ctx().seed_rng(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}
// 双侧镜像：双方各选 0 号（owner=0/1 两键段隔离直证用）。
void run_cast_mirror(ScenarioBattle& b, int seed) {
    b.ctx().seed_rng(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}
// 双侧 NONE 推进一回合（窗界腿用）。
void run_idle_round(ScenarioBattle& b, int seed) {
    b.ctx().seed_rng(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 对手 Modern 层异常免疫（3 回合）——1498 landed==0 的造料（890/8260 同款注入）。
void grant_foe_anomaly_immune(ScenarioBattle& b) {
    b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/3, /*source_id=*/990, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/990,
                           /*tier=*/ImmunityTier::Modern);
}

// 池改写票：任意池缩到给成员（084/8250/8260 同款注入；票全局折叠）。
void grant_pool_mod(ScenarioBattle& b, std::vector<int> members) {
    b.ctx().rule_center_.grant_anomaly_pool_mod(
        0, 9001, [members](BattleContext*, const std::vector<int>&) { return members; });
}

// 池改写恒等观测票：原样返回入参池，把 1498 命名池成员在场性记进 plugin_storage
// ——插件命名池边界的种子无关直证（T2 专用；键段 kPoolObs，owner=0）。
void grant_pool_observer(ScenarioBattle& b) {
    b.ctx().rule_center_.grant_anomaly_pool_mod(
        0, 9002, [](BattleContext* c, const std::vector<int>& pool) {
            const int key0 = 2092498000;
            auto set = [&](int slot, int v) { c->plugin_storage[key0 + slot] = v; };
            set(0, static_cast<int>(pool.size()));
            for (int slot = 1; slot <= 5; ++slot) set(slot, 0);
            for (int id : pool) {
                if (id == 2) set(1, 1);        // 烧伤在池
                else if (id == 5) set(2, 1);   // 冻伤在池
                else if (id == 20) set(3, 1);  // 失明在池
                else if (id == 29) set(4, 1);  // 失神在池
                else if (id == 1) set(5, 1);   // 中毒不在池（对照位，恒 0 才对）
            }
            return pool;
        });
}

// 弱化/提升造料（原语直调：本体/视图一并同步）。
void drop_self(ScenarioBattle& b, int stat, int amount) {
    REQUIRE(stat_drop(&b.ctx(), 0, stat, amount, /*actor=*/0) == StatDropResult::SUCCESS);
}
void drop_foe(ScenarioBattle& b, int stat, int amount) {
    REQUIRE(stat_drop(&b.ctx(), 1, stat, amount, /*actor=*/0) == StatDropResult::SUCCESS);
}
void boost_foe(ScenarioBattle& b, int stat, int delta) {
    REQUIRE(stat_change(&b.ctx(), 1, stat, delta) == StatChangeResult::SUCCESS);
}
void boost_self(ScenarioBattle& b, int stat, int delta) {
    REQUIRE(stat_change(&b.ctx(), 0, stat, delta) == StatChangeResult::SUCCESS);
}

int level(BattleContext& ctx, int side, int slot) { return ctx.ability_levels[side][slot]; }

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //===== T1 1498 触发档：36515 [2,1,100] + 缩池 {2,5} → 恰落 2 种、补偿不授予 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kXinghan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_pool_mod(b, {2, 5});
        run_cast(b, 0, 20261109);
        CHECK_EQ(active_anomaly_count(ctx, 1), 2, "T1 核心：恰落 2 种（缩池两成员）");
        CHECK_EQ(a1498(ctx, 0, 0), 2, "T1 申报种类数=2（行参原序首参）");
        CHECK_EQ(a1498(ctx, 0, 1), 2, "T1 requested=2");
        CHECK_EQ(a1498(ctx, 0, 2), 1, "T1 rolled=1（100 申报短路必过）");
        CHECK_EQ(a1498(ctx, 0, 3), 2, "T1 landed=2");
        CHECK_EQ(a1498(ctx, 0, 4), 2, "T1 新落 id 槽4=烧伤2");
        CHECK_EQ(a1498(ctx, 0, 5), 5, "T1 新落 id 槽5=冻伤5（id 升序）");
        CHECK_EQ(a1498(ctx, 0, 6), -1, "T1 差集只有 2 项（槽6 未用）");
        CHECK_EQ(a1498(ctx, 0, 10), 0, "T1 触发档：AMP 窗不授予");
        CHECK_EQ(a1498(ctx, 0, 11), 0, "T1 窗回合数清账");
        CHECK_EQ(a1498(ctx, 0, 12), 0, "T1 窗 pct 清账");
        CHECK_EQ(a1498(ctx, 0, 8), 1, "T1 使用计数=1");
        CHECK_EQ(a1498(ctx, 0, 9), 0, "T1 无跳过");
        std::cout << "  [T1] 1498 缩池恰落2种+审计吻合+补偿不授予 通过\n";
    }

    //===== T2 1498 命名四元素池边界：恒等观测票（种子无关直证）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kXinghan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_pool_observer(b);
        run_cast(b, 0, 777);
        const auto get = [&](int slot) {
            const auto it = ctx.plugin_storage.find(2092498000 + slot);
            if (it == ctx.plugin_storage.end()) return -99;
            const int* v = std::any_cast<int>(&it->second);
            return v ? *v : -99;
        };
        CHECK_EQ(get(0), 4, "T2 核心：命名池大小=4");
        CHECK_EQ(get(1), 1, "T2 烧伤2 在池");
        CHECK_EQ(get(2), 1, "T2 冻伤5 在池");
        CHECK_EQ(get(3), 1, "T2 失明20 在池");
        CHECK_EQ(get(4), 1, "T2 失神29 在池");
        CHECK_EQ(get(5), 0, "T2 中毒1 不在池（对照位）");
        CHECK_EQ(a1498(ctx, 0, 0), 2, "T2 申报种类数=2（36515 行参切片 [2,1,100]）");
        CHECK_EQ(a1498(ctx, 0, 3), 2, "T2 landed=2（原池恰附加 2 种，计数种子无关）");
        CHECK_EQ(active_anomaly_count(ctx, 1), 2, "T2 对手恰带 2 种异常（成员必属四元素池）");
        std::cout << "  [T2] 1498 命名池边界观测（{2,5,20,29}） 通过\n";
    }

    //===== T3 1498 补偿档：对手 Modern 免疫 → landed=0 → AMP 窗授予 + R+1 字段断言 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kXinghan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_anomaly_immune(b);
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1498(ctx, 0, 3), 0, "T3 核心：landed=0（免疫拦，apply 层全挡）");
        CHECK_EQ(active_anomaly_count(ctx, 1), 0, "T3 对手零异常（免疫）");
        CHECK_EQ(a1498(ctx, 0, 10), 1, "T3 补偿档：AMP 窗授予");
        CHECK_EQ(a1498(ctx, 0, 11), 1, "T3 窗回合数={1} 行参=1");
        CHECK_EQ(a1498(ctx, 0, 12), 100, "T3 窗 pct={2} 行参=100");
        // R1（窗内）：ROUND_START tick 写 ws.damage_add_pct[0]，ws 只在 ROUND_START 重置
        // → R1 跑完后字段仍持有本回合写入量（post-drive 直读合法）。
        run_idle_round(b, 111);
        const float amp_r1 = ctx.ws.damage_add_pct[0];
        CHECK(amp_r1 > 99.9f && amp_r1 < 100.1f, "T3 R+1 字段断言：damage_add_pct=100（窗内）");
        std::cout << "  [T3] 1498 免疫补偿→AMP窗授予+R+1字段=100 通过\n";
    }

    //===== T4 1498 窗界：36679 [2,1,50] → R+1=50、R+2=0（NextRounds 过期）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kShenying, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_anomaly_immune(b);
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1498(ctx, 0, 10), 1, "T4 补偿档授予");
        CHECK_EQ(a1498(ctx, 0, 12), 50, "T4 窗 pct=50（36679 行参变体）");
        run_idle_round(b, 112);
        float amp_r1 = ctx.ws.damage_add_pct[0];
        CHECK(amp_r1 > 49.9f && amp_r1 < 50.1f, "T4 R+1 字段断言：damage_add_pct=50");
        run_idle_round(b, 113);   // R2：窗已过期（rounds=1），ROUND_START 重置后无写入
        amp_r1 = ctx.ws.damage_add_pct[0];
        CHECK(amp_r1 > -0.1f && amp_r1 < 0.1f, "T4 核心：R+2 窗过期归零（NextRounds 界）");
        std::cout << "  [T4] 1498 窗界 R+1=50 / R+2=0（pct 行参变体） 通过\n";
    }

    //===== T5 1134 成功档：自身两下降槽 → 镜像落地 + 自身清 + 无全属性补偿 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kGuangying, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        drop_self(b, 0, 2);   // 自身攻击-2
        drop_self(b, 4, 1);   // 自身速度-1
        REQUIRE(level(ctx, 0, 0) == -2 && level(ctx, 0, 4) == -1);
        run_cast(b, 0, 20261109);
        CHECK_EQ(level(ctx, 1, 0), -2, "T5 核心：镜像落地 对手攻击-2");
        CHECK_EQ(level(ctx, 1, 4), -1, "T5 镜像落地 对手速度-1");
        CHECK_EQ(level(ctx, 0, 0), 0, "T5 自身攻击清零（反馈解除半边）");
        CHECK_EQ(level(ctx, 0, 4), 0, "T5 自身速度清零");
        CHECK_EQ(level(ctx, 0, 2), 0, "T5 成功档无全属性补偿（防御槽仍 0）");
        CHECK_EQ(a1134(ctx, 0, 0), 1, "T5 行参 n=1");
        CHECK_EQ(a1134(ctx, 0, 1), 2, "T5 自身下降槽快照数=2");
        CHECK_EQ(a1134(ctx, 0, 2), 2, "T5 镜像落地槽数=2（SUCCESS 计数）");
        CHECK_EQ(a1134(ctx, 0, 3), 1, "T5 反馈判定=成功");
        CHECK_EQ(a1134(ctx, 0, 4), 0, "T5 未被锁");
        CHECK_EQ(a1134(ctx, 0, 5), 1, "T5 成功分支触发");
        CHECK_EQ(a1134(ctx, 0, 6), 0, "T5 失败分支未触发");
        CHECK_EQ(a1134(ctx, 0, 7), 1, "T5 自身清下降发生");
        std::cout << "  [T5] 1134 成功档：镜像+清自身+无补偿 通过\n";
    }

    //===== T6 1134 失败档（无可反馈）：自身全属性+1 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kGuangying, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1134(ctx, 0, 1), 0, "T6 快照数=0（自身无下降）");
        CHECK_EQ(a1134(ctx, 0, 3), 2, "T6 反馈判定=失败");
        CHECK_EQ(a1134(ctx, 0, 6), 1, "T6 失败分支触发");
        CHECK_EQ(a1134(ctx, 0, 7), 0, "T6 无清下降（无可清）");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(level(ctx, 0, i), 1, "T6 核心：失败分支全属性六槽各+1（逐槽）");
        }
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(level(ctx, 1, i), 0, "T6 对手未被镜像（无可反馈）");
        }
        std::cout << "  [T6] 1134 失败档（无可反馈）：全属性+1 逐槽 通过\n";
    }

    //===== T7 1134 失败档（对手 AT_FLOOR）：镜像不落地 → 全属性+1、自身不清（63 原子）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kGuangying, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        drop_foe(b, 2, 6);    // 对手防御压到 -6
        REQUIRE(stat_drop(&b.ctx(), 1, 2, 1, 0) == StatDropResult::AT_FLOOR);
        drop_self(b, 2, 2);   // 自身防御-2（唯一负槽 → 镜像必撞 AT_FLOOR）
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1134(ctx, 0, 2), 0, "T7 核心：镜像落地槽数=0（AT_FLOOR 入未触发档）");
        CHECK_EQ(a1134(ctx, 0, 3), 2, "T7 反馈判定=失败");
        CHECK_EQ(level(ctx, 1, 2), -6, "T7 对手防御仍 -6（未再降）");
        CHECK_EQ(level(ctx, 0, 2), -1, "T7 核心：自身防御 -2+1=-1（失败不清、补偿照加）");
        CHECK_EQ(level(ctx, 0, 0), 1, "T7 其余槽 +1");
        CHECK_EQ(a1134(ctx, 0, 7), 0, "T7 自身清下降未发生（63 原子裁定）");
        std::cout << "  [T7] 1134 AT_FLOOR 失败档：补偿+原子不清 通过\n";
    }

    //===== T8 1134 失败档（自身被锁）：反馈整体失败（63 batch_b:317 同款）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kGuangying, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        drop_self(b, 0, 2);   // 自身攻击-2
        ctx.grant_immunity(0, ImmunityType::STAT_DROP_LOCK, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/2, /*source_id=*/9902, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/9902);
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1134(ctx, 0, 4), 1, "T8 核心：自身被锁识别");
        CHECK_EQ(a1134(ctx, 0, 3), 2, "T8 反馈判定=失败");
        CHECK_EQ(a1134(ctx, 0, 2), 0, "T8 镜像未尝试（原子失败）");
        CHECK_EQ(level(ctx, 1, 0), 0, "T8 对手未被镜像");
        CHECK_EQ(level(ctx, 0, 0), -1, "T8 核心：自身攻击 -2+1=-1（锁住不清、补偿照加）");
        CHECK_EQ(a1134(ctx, 0, 7), 0, "T8 清下降未发生");
        std::cout << "  [T8] 1134 被锁失败档：原子失败+补偿 通过\n";
    }

    //===== T9 1359 成功档：镜像 + 清自身 + 全属性+1 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kLingjian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        drop_self(b, 0, 2);   // 自身攻击-2（唯一负槽）
        run_cast(b, 0, 20261109);
        CHECK_EQ(level(ctx, 1, 0), -2, "T9 核心：镜像落地 对手攻击-2");
        CHECK_EQ(a1359(ctx, 0, 3), 1, "T9 反馈判定=成功");
        CHECK_EQ(a1359(ctx, 0, 5), 1, "T9 成功分支触发");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(level(ctx, 0, i), 1, "T9 核心：成功分支全属性+1 且下降已清（逐槽）");
        }
        CHECK_EQ(a1359(ctx, 0, 7), 1, "T9 清下降发生");
        std::cout << "  [T9] 1359 成功档：镜像+清自身+全属性+1 通过\n";
    }

    //===== T10 1359 失败档（对手 AT_FLOOR）：解除自身、不提升（与 1134 分道）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kLingjian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        drop_foe(b, 0, 6);    // 对手攻击 -6（AT_FLOOR）
        drop_self(b, 0, 2);   // 自身攻击-2（唯一负槽 → 镜像必撞 AT_FLOOR）
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1359(ctx, 0, 2), 0, "T10 核心：镜像落地=0");
        CHECK_EQ(a1359(ctx, 0, 3), 2, "T10 反馈判定=失败");
        CHECK_EQ(a1359(ctx, 0, 6), 1, "T10 失败分支触发");
        CHECK_EQ(level(ctx, 0, 0), 0, "T10 核心：失败分支解除自身（-2→0）");
        CHECK_EQ(level(ctx, 0, 2), 0, "T10 无提升（防御槽仍 0——失败分支只解除不+1）");
        CHECK_EQ(a1359(ctx, 0, 7), 1, "T10 失败分支清下降发生（1359 显式解除）");
        std::cout << "  [T10] 1359 AT_FLOOR 失败档：解除+不提升 通过\n";
    }

    //===== T11 1771 成功档：反转 + 票挂对手 + 锁功能直证 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kJingye, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        boost_foe(b, 0, 2);   // 对手攻击+2
        boost_foe(b, 4, 1);   // 对手速度+1
        run_cast(b, 0, 20261109);
        CHECK_EQ(level(ctx, 1, 0), -2, "T11 核心：反转 对手攻击+2→-2");
        CHECK_EQ(level(ctx, 1, 4), -1, "T11 反转 对手速度+1→-1");
        CHECK_EQ(a1771(ctx, 0, 1), 1, "T11 反转结果=REVERSED");
        CHECK_EQ(a1771(ctx, 0, 2), 1, "T11 票授予");
        CHECK_EQ(a1771(ctx, 0, 0), 2, "T11 行参 rounds=2（切片 [2]）");
        CHECK_EQ(a1771(ctx, 0, 3), 2, "T11 票回合数=2");
        CHECK(ctx.is_immune(1, ImmunityType::STAT_DROP_LOCK, ctx.currentState),
              "T11 对手持 STAT_DROP_LOCK 票");
        CHECK_EQ(clear_stat_drops(&b.ctx(), 1), 0, "T11 功能直证：clear_stat_drops 被锁返 0");
        CHECK(stat_reversal(&b.ctx(), 1) == StatReversalResult::BLOCKED,
              "T11 功能直证：stat_reversal 被锁返 BLOCKED");
        std::cout << "  [T11] 1771 成功档：反转+锁票功能直证 通过\n";
    }

    //===== T12 1771 NOTHING 档：对手无提升 → 不授票 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kJingye, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1771(ctx, 0, 1), 2, "T12 核心：反转结果=NOTHING（无提升）");
        CHECK_EQ(a1771(ctx, 0, 2), 0, "T12 不授票");
        CHECK(!ctx.is_immune(1, ImmunityType::STAT_DROP_LOCK, ctx.currentState),
              "T12 对手无 STAT_DROP_LOCK 票");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(level(ctx, 1, i), 0, "T12 对手等级未动");
        }
        std::cout << "  [T12] 1771 NOTHING 档：不授票 通过\n";
    }

    //===== T13 1771 BLOCKED 档：对手免弱 → 反转被挡、不授票 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kJingye, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        boost_foe(b, 0, 2);   // 对手攻击+2
        ctx.grant_immunity(1, ImmunityType::STAT_DROP, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/2, /*source_id=*/9903, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/9903);
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1771(ctx, 0, 1), 3, "T13 核心：反转结果=BLOCKED（免弱挡反转）");
        CHECK_EQ(a1771(ctx, 0, 2), 0, "T13 不授票");
        CHECK_EQ(level(ctx, 1, 0), 2, "T13 对手攻击+2 原样（等级一点不动）");
        std::cout << "  [T13] 1771 BLOCKED 档：免弱挡反转+不授票 通过\n";
    }

    //===== T14 1771 切片 [3]：修罗百破击（K438「全[2]」纠错载体）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kXiuluo, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        boost_foe(b, 0, 2);   // 对手攻击+2
        run_cast(b, 0, 20261109);
        CHECK_EQ(level(ctx, 1, 0), -2, "T14 反转落地（对手攻击+2→-2）");
        CHECK_EQ(a1771(ctx, 0, 0), 3, "T14 核心：行参 rounds=3（链 [2554,881,835,1771,1821] 平面切片 [3]——K438 记档纠错）");
        CHECK_EQ(a1771(ctx, 0, 2), 1, "T14 票授予");
        CHECK_EQ(a1771(ctx, 0, 3), 3, "T14 票回合数=3");
        std::cout << "  [T14] 1771 切片[3]载体（修罗百破击） 通过\n";
    }

    //===== T15 1146 计数主正例：3 提升态 × 50 = 150 + 审计吻合 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kHuaguang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        boost_self(b, 4, 1);  // 自身速度+1（1 种）
        boost_foe(b, 4, 1);   // 对手速度+1（2 种）
        boost_foe(b, 5, 1);   // 对手命中+1（3 种；速/命不入伤害公式、不翻先手）
        const int hp0 = b.pet(1).hp;
        run_cast(b, 0, 20261109);
        const int loss = hp0 - b.pet(1).hp;
        CHECK(loss >= 150, "T15 对手掉血 ≥ 150（攻击伤害 + 固伤 150）");
        CHECK_EQ(a1146(ctx, 0, 0), 1, "T15 行参 {0}=1（全族恒 1，在位即消费）");
        CHECK_EQ(a1146(ctx, 0, 1), 50, "T15 行参 {1}=50");
        CHECK_EQ(a1146(ctx, 0, 2), 1, "T15 自身提升态种数=1");
        CHECK_EQ(a1146(ctx, 0, 3), 2, "T15 对手提升态种数=2");
        CHECK_EQ(a1146(ctx, 0, 4), 3, "T15 总种数=3");
        CHECK_EQ(a1146(ctx, 0, 5), 150, "T15 核心：附加固伤实参=3×50=150");
        CHECK_EQ(a1146(ctx, 0, 8), 1, "T15 使用计数=1");
        std::cout << "  [T15] 1146 计数主正例（3 态×50=150） 通过\n";
    }

    //===== T16 1146 差分：同 seed 双局，对手提升态 1→2 种，掉血差恰 = {1}=50 =====
    {
        int loss_a = -1, loss_b = -1;
        {
            ScenarioBattle b(scenario::make_party(caster({kHuaguang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            boost_foe(b, 4, 1);   // 对手 1 种
            const int hp0 = b.pet(1).hp;
            run_cast(b, 0, 555);
            loss_a = hp0 - b.pet(1).hp;
            CHECK_EQ(a1146(b.ctx(), 0, 5), 50, "T16 A 局固伤=1×50");
        }
        {
            ScenarioBattle b(scenario::make_party(caster({kHuaguang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            boost_foe(b, 4, 1);   // 对手 2 种（速+命中，均不入伤害公式）
            boost_foe(b, 5, 1);
            const int hp0 = b.pet(1).hp;
            run_cast(b, 0, 555);  // 同 seed → 攻击伤害 roll 逐位同
            loss_b = hp0 - b.pet(1).hp;
            CHECK_EQ(a1146(b.ctx(), 0, 5), 100, "T16 B 局固伤=2×50");
        }
        CHECK_EQ(loss_b - loss_a, 50, "T16 核心：同 seed 差分恰 +50（固伤随种数线性、攻击分量同）");
        std::cout << "  [T16] 1146 同 seed 差分（+50） 通过\n";
    }

    //===== T17 1146 零计数：双侧无提升态 → 固伤 0（fixed_damage 不调用）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kHuaguang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1146(ctx, 0, 4), 0, "T17 总种数=0");
        CHECK_EQ(a1146(ctx, 0, 5), 0, "T17 核心：固伤实参=0（不附加）");
        CHECK_EQ(a1146(ctx, 0, 9), 0, "T17 无跳过（宏正常跑完）");
        std::cout << "  [T17] 1146 零计数档 通过\n";
    }

    //===== T18 1146 amount 行参变体：33316 [1,40] → 2 态 × 40 = 80 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kHuangsha, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        boost_foe(b, 4, 1);
        boost_foe(b, 5, 1);   // 对手 2 种
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1146(ctx, 0, 1), 40, "T18 行参 {1}=40（33316 变体）");
        CHECK_EQ(a1146(ctx, 0, 5), 80, "T18 核心：附加固伤=2×40=80（amount 行参消费）");
        std::cout << "  [T18] 1146 amount 变体（2 态×40=80） 通过\n";
    }

    //===== T19 1146 owner 隔离：镜像对轰 → owner=0/1 两键段各写各的 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kHuaguang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kHuaguang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        boost_foe(b, 4, 1);   // 对手 1 种
        run_cast_mirror(b, 666);
        CHECK_EQ(a1146(ctx, 0, 8), 1, "T19 owner0 使用计数=1");
        CHECK_EQ(a1146(ctx, 1, 8), 1, "T19 核心：owner1 使用计数=1（bind_participants 换向）");
        CHECK_EQ(a1146(ctx, 0, 2), 0, "T19 owner0 视角自身种数=0");
        CHECK_EQ(a1146(ctx, 0, 3), 1, "T19 owner0 视角对手种数=1");
        CHECK_EQ(a1146(ctx, 1, 2), 1, "T19 核心：owner1 视角自身种数=1（提升在 side1）");
        CHECK_EQ(a1146(ctx, 1, 3), 0, "T19 owner1 视角对手种数=0（与 owner0 互换）");
        CHECK_EQ(a1146(ctx, 0, 4), 1, "T19 owner0 总种数=1");
        CHECK_EQ(a1146(ctx, 1, 4), 1, "T19 owner1 总种数=1（两侧同账）");
        CHECK_EQ(a1146(ctx, 0, 5), 50, "T19 owner0 固伤=1×50");
        CHECK_EQ(a1146(ctx, 1, 5), 50, "T19 owner1 固伤=1×50");
        std::cout << "  [T19] 1146 owner 隔离（镜像对轰两键段） 通过\n";
    }

    std::cout << "scenario_8270: ALL PASS（T1-T19）" << std::endl;
    return 0;
}
