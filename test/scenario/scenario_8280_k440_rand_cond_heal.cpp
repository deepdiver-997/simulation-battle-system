// 场景 8280：K440 探索七批自选三族 —— 75「命中时有30%概率令对手麻痹、睡眠或害怕」×6
// + 737「{0}%使对手{1}，若没有触发，则回合结束时减少对手1/{2}最大体力」×6 +
// 734「若对手不处于能力提升或下降状态时则恢复{0}点体力」×6
//（resources/moves_lib/k440_rand_cond_heal_pool.cpp，2026-11-09；号段 8280-8289 一文件
//  覆盖三族，scenario_8270 同式）
//
// 断言口径（详见插件头注）：三宏全落 SKILL_EFFECT 桶。
//   75 通道 = attach_random_anomalies（K437 851 / K439 1498 命名池直系）+ 零参宏
//     P=30 写死。随机面固定两腿：亮节闸门 force-open/close（8250/8270 同款）+
//     Modern 免疫造 landed==0；命名池边界用**恒等观测票**种子无关直证（8270 T2
//     同式）。落地 id 断言只锁 ∈ 三池，不锁具体 id（种子无关）。
//   737 通道 = apply_anomaly_checked（K422 917 申报制直系）+ 未触发 → AFTER_ACTION
//     单发兜底（J50 644 同形，未击败门）。触发/掷败/免疫/击杀四腿：兜底量经审计槽
//     断言 == 对手最大体力/{2} 精确值（固伤落血由 fixed_damage 一等公民承担——
//     1146/644 场景既验，不对攻击伤害公式噪声做脆断言）。
//   734 通道 = 六槽净空门（K439 1146 同表遍历）+ heal_amount 原语。开/闭两腿 +
//     提升腿/下降腿/命中槽腿 + 变体量腿；回血观测先压自身当前体力（坑集 §4/§9：
//     满血观测会被 clamp 掩盖）。
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

constexpr int kQise     = 11138;  // 七色光芒 [75]：特75（75 纯独载体）
constexpr int kRaoluan   = 15480; // 扰乱侵袭 [74,75]：物70（75 链中位载体，74 在册）
constexpr int kZhuxian   = 17133; // 诛仙灭神斩 [737][30,8,5]：特160 必中（737 纯独载体）
constexpr int kLonghun   = 17269; // 龙魂焚火咒 [737][50,2,4]：特160（737 denom 变体）
constexpr int kFeihong   = 19454; // 绯红破空杀 [737,903][50,8,4,30]：特160（737 链中位，903 在册）
constexpr int kTasa      = 30267; // 踏飒流星 [734][200]：物130（734 纯独载体）
constexpr int kDimo      = 17129; // 地魔穿心箭 [734,735][150,1,1]：物160 必中（734 混链载体，
                                  //  735 未注册保持 no-op——K438 1624 混链记档同口径）
constexpr int kPaiDa     = 10004; // 拍打：物40 无效果填充刀

constexpr int kAudit75  = 1000750000;    // K440 75 键段
constexpr int kAudit737 = 1007370000;    // K440 737 键段
constexpr int kAudit734 = 1007340000;    // K440 734 键段
constexpr int kPoolObs  = 2092568000;    // T3 恒等观测票落点（场景私有 2.092568B 段）

int audit_int(BattleContext& ctx, int base, int owner, int slot, int def = -99) {
    const auto it = ctx.plugin_storage.find(base + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
int a75(BattleContext& ctx, int owner, int slot) { return audit_int(ctx, kAudit75, owner, slot); }
int a737(BattleContext& ctx, int owner, int slot) { return audit_int(ctx, kAudit737, owner, slot); }
int a734(BattleContext& ctx, int owner, int slot) { return audit_int(ctx, kAudit734, owner, slot); }

int active_anomaly_count(BattleContext& ctx, int side) {
    int n = 0;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) ++n;
    }
    return n;
}
std::vector<int> active_anomaly_ids(BattleContext& ctx, int side) {
    std::vector<int> ids;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) ids.push_back(id);
    }
    return ids;
}
bool in_named_three(int id) {
    return id == static_cast<int>(AbnormalStatusId::Paralysis) ||
           id == static_cast<int>(AbnormalStatusId::Sleep) ||
           id == static_cast<int>(AbnormalStatusId::Fear);
}

ElfPet caster(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = scenario::make_pet(0, "施加方", skills, speed, /*atk=*/300, 400000);
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

// R1 骨架：我方出载体，对手无还手。
void run_cast(ScenarioBattle& b, int skill_index, int seed) {
    sbs_srand(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 亮节闸门 force-open / force-close（防守侧持票；AnomalyAttach × Skill 源。8250 同款）。
void grant_gate(ScenarioBattle& b, int above_result) {
    b.ctx().grant_chance_gate(/*owner=*/1, /*source_effect_id=*/9901,
                              ChanceTag::AnomalyAttach, /*threshold_pct=*/0,
                              /*below_result=*/-1, /*above_result=*/above_result,
                              /*source_mask=*/chance_source_bit(ChanceSource::Skill),
                              /*rounds=*/0);
}

// 对手 Modern 层异常免疫（3 回合）——landed==0 的造料（890/8260/8270 同款注入）。
void grant_foe_anomaly_immune(ScenarioBattle& b) {
    b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/3, /*source_id=*/990, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/990,
                           /*tier=*/ImmunityTier::Modern);
}

// 池改写恒等观测票：原样返回入参池，把 75 命名池成员在场性记进 plugin_storage
// ——插件命名池边界的种子无关直证（T3 专用；键段 kPoolObs，owner=0）。
void grant_pool_observer(ScenarioBattle& b) {
    b.ctx().rule_center_.grant_anomaly_pool_mod(
        0, 9002, [](BattleContext* c, const std::vector<int>& pool) {
            const int key0 = kPoolObs;
            auto set = [&](int slot, int v) { c->plugin_storage[key0 + slot] = v; };
            set(0, static_cast<int>(pool.size()));
            for (int slot = 1; slot <= 4; ++slot) set(slot, 0);
            for (int id : pool) {
                if (id == 0) set(1, 1);        // 麻痹在池
                else if (id == 8) set(2, 1);   // 睡眠在池
                else if (id == 6) set(3, 1);   // 害怕在池
                else if (id == 1) set(4, 1);   // 中毒不在池（对照位，恒 0 才对）
            }
            return pool;
        });
}

// 弱化/提升造料（原语直调：本体/视图一并同步）。
void drop_foe(ScenarioBattle& b, int stat, int amount) {
    REQUIRE(stat_drop(&b.ctx(), 1, stat, amount, /*actor=*/0) == StatDropResult::SUCCESS);
}
void boost_foe(ScenarioBattle& b, int stat, int delta) {
    REQUIRE(stat_change(&b.ctx(), 1, stat, delta) == StatChangeResult::SUCCESS);
}

int level(BattleContext& ctx, int side, int slot) { return ctx.ability_levels[side][slot]; }

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //===== T1 75 主正例（force-open）：11138 → 恰落 1 种 ∈ 三池 + 审计吻合 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kQise, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_gate(b, /*above_result=*/100);
        run_cast(b, 0, 20261109);
        const std::vector<int> ids = active_anomaly_ids(ctx, 1);
        CHECK_EQ(active_anomaly_count(ctx, 1), 1, "T1 核心：恰落 1 种异常（闸门 force-open）");
        REQUIRE(ids.size() == 1);
        CHECK(in_named_three(ids[0]), "T1 落地 id 必 ∈ {麻痹0,睡眠8,害怕6}");
        CHECK_EQ(a75(ctx, 0, 0), 30, "T1 申报概率=30（零参宏写死值）");
        CHECK_EQ(a75(ctx, 0, 1), 1, "T1 requested=1");
        CHECK_EQ(a75(ctx, 0, 2), 1, "T1 rolled=1（闸门改写必过）");
        CHECK_EQ(a75(ctx, 0, 3), 1, "T1 landed=1");
        CHECK_EQ(a75(ctx, 0, 4), ids[0], "T1 新落 id 槽4=场上唯一异常");
        CHECK_EQ(a75(ctx, 0, 5), -1, "T1 差集只有 1 项（槽5 未用）");
        CHECK_EQ(a75(ctx, 0, 8), 1, "T1 使用计数=1");
        CHECK_EQ(a75(ctx, 0, 9), 0, "T1 无跳过");
        std::cout << "  [T1] 75 force-open 恰落1种∈三池 通过\n";
    }

    //===== T2 75 force-close：rolled=0 → landed=0、无兜底、场上净空 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kQise, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_gate(b, /*above_result=*/0);
        run_cast(b, 0, 20261109);
        CHECK_EQ(active_anomaly_count(ctx, 1), 0, "T2 核心：无异常落地");
        CHECK_EQ(a75(ctx, 0, 0), 30, "T2 申报概率=30");
        CHECK_EQ(a75(ctx, 0, 1), 1, "T2 requested=1");
        CHECK_EQ(a75(ctx, 0, 2), 0, "T2 核心：rolled=0（闸门改写必败）");
        CHECK_EQ(a75(ctx, 0, 3), 0, "T2 landed=0（75 无兜底子句，什么都不做）");
        CHECK_EQ(a75(ctx, 0, 8), 1, "T2 使用计数=1");
        std::cout << "  [T2] 75 force-close 零落地 通过\n";
    }

    //===== T3 75 命名池边界：恒等观测票直证池恰 = {麻痹0,睡眠8,害怕6}（种子无关）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kQise, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_pool_observer(b);
        grant_gate(b, 100);
        run_cast(b, 0, 20261109);
        auto obs = [&](int slot) {
            const auto it = ctx.plugin_storage.find(kPoolObs + slot);
            const int* v = it == ctx.plugin_storage.end() ? nullptr : std::any_cast<int>(&it->second);
            return v ? *v : -99;
        };
        CHECK_EQ(obs(0), 3, "T3 核心：池大小=3");
        CHECK_EQ(obs(1), 1, "T3 麻痹0 在池");
        CHECK_EQ(obs(2), 1, "T3 睡眠8 在池");
        CHECK_EQ(obs(3), 1, "T3 害怕6 在池");
        CHECK_EQ(obs(4), 0, "T3 对照位：中毒1 不在池（六池变体载体 15480 文不同——模板权威）");
        CHECK_EQ(a75(ctx, 0, 3), 1, "T3 观测票恒等折叠不改变落地账");
        std::cout << "  [T3] 75 命名池边界（3 元素直证） 通过\n";
    }

    //===== T4 75 免疫腿：rolled=1、landed=0（Modern 免疫拦；无兜底）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kQise, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_anomaly_immune(b);
        grant_gate(b, 100);
        run_cast(b, 0, 20261109);
        CHECK_EQ(active_anomaly_count(ctx, 1), 0, "T4 核心：免疫拦下，场上净空");
        CHECK_EQ(a75(ctx, 0, 2), 1, "T4 rolled=1（闸门开）");
        CHECK_EQ(a75(ctx, 0, 3), 0, "T4 核心：landed=0（免疫拦在 apply 层，K437 T7 同口径）");
        std::cout << "  [T4] 75 免疫腿 landed=0 通过\n";
    }

    //===== T5 75 链中位载体：15480 [74,75]（74 在册先行、75 零参随链照跑）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kRaoluan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_gate(b, 100);
        run_cast(b, 0, 20261109);
        CHECK_EQ(a75(ctx, 0, 8), 1, "T5 核心：链中位 75 照常记账（使用计数=1）");
        CHECK_EQ(a75(ctx, 0, 3), 1, "T5 landed=1");
        // 链兄弟 74「命中时有30%概率令对手中毒、烧伤或冻伤」（j_random_trio_anomaly
        // 在册）同吃 force-open 闸门、各自落地——计数只锁三池成员面。
        int named_three_n = 0;
        for (int id : active_anomaly_ids(ctx, 1)) {
            if (in_named_three(id)) ++named_three_n;
        }
        CHECK_EQ(named_three_n, 1, "T5 核心：三池面恰 1 异常（74 的毒烧冻面另计）");
        CHECK(in_named_three(a75(ctx, 0, 4)), "T5 落地 id ∈ 三池");
        CHECK_EQ(a75(ctx, 0, 4), active_anomaly_ids(ctx, 1).empty() ? -99 : [&]{
            for (int id : active_anomaly_ids(ctx, 1)) if (in_named_three(id)) return id;
            return -99; }(), "T5 新落 id 槽4 = 三池面在场上唯一异常");
        std::cout << "  [T5] 75 链中位载体（扰乱侵袭） 通过\n";
    }

    //===== T6 737 触发档：17133 [30,8,5] + force-open → 睡眠落地、兜底不 armed =====
    {
        ScenarioBattle b(scenario::make_party(caster({kZhuxian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_gate(b, 100);
        run_cast(b, 0, 20261109);
        CHECK(ctx.has_active_abnormal_status(1, 8), "T6 核心：睡眠8 落地");
        CHECK_EQ(a737(ctx, 0, 0), 30, "T6 行参 P=30");
        CHECK_EQ(a737(ctx, 0, 1), 8, "T6 行参 异常码=8（平面切片原序）");
        CHECK_EQ(a737(ctx, 0, 2), 5, "T6 行参 分母=5");
        CHECK_EQ(a737(ctx, 0, 3), 1, "T6 核心：触发判定=1（四档落地账）");
        CHECK_EQ(a737(ctx, 0, 4), 0, "T6 核心：兜底不 armed（触发档显式清账）");
        CHECK_EQ(a737(ctx, 0, 5), 0, "T6 兜底未 fired");
        CHECK_EQ(a737(ctx, 0, 6), 0, "T6 兜底量清账");
        CHECK_EQ(a737(ctx, 0, 8), 1, "T6 使用计数=1");
        std::cout << "  [T6] 737 触发档（睡眠落地+兜底不装填） 通过\n";
    }

    //===== T7 737 掷败档：force-close → ROLL_FAILED → 兜底 fired、量=600000/5 精确 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kZhuxian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_gate(b, /*above_result=*/0);
        const int hp0 = b.pet(1).hp;
        run_cast(b, 0, 20261109);
        const int loss = hp0 - b.pet(1).hp;
        CHECK_EQ(a737(ctx, 0, 3), 0, "T7 核心：触发判定=0（ROLL_FAILED 入未触发档）");
        CHECK_EQ(a737(ctx, 0, 4), 1, "T7 核心：兜底 armed");
        CHECK_EQ(a737(ctx, 0, 5), 1, "T7 核心：兜底 fired（AFTER_ACTION 单发）");
        CHECK_EQ(a737(ctx, 0, 6), 120000, "T7 核心：兜底量=对手最大体力1/5=120000 精确");
        CHECK(loss >= 120000, "T7 对手掉血 ≥ 兜底量（攻击伤害另计，方向腿）");
        std::cout << "  [T7] 737 掷败档（兜底 fired+量精确） 通过\n";
    }

    //===== T8 737 免疫档：force-open + Modern 免疫 → TARGET_IMMUNE 入未触发 → 兜底 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kZhuxian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_anomaly_immune(b);
        grant_gate(b, 100);
        run_cast(b, 0, 20261109);
        CHECK_EQ(active_anomaly_count(ctx, 1), 0, "T8 免疫拦下无异常");
        CHECK_EQ(a737(ctx, 0, 3), 0, "T8 核心：TARGET_IMMUNE 入未触发档");
        CHECK_EQ(a737(ctx, 0, 5), 1, "T8 核心：兜底 fired");
        CHECK_EQ(a737(ctx, 0, 6), 120000, "T8 兜底量=120000");
        std::cout << "  [T8] 737 免疫档（兜底照发） 通过\n";
    }

    //===== T9 737 denom 变体：17269 [50,2,4] + 免疫 → 兜底量=600000/4=150000 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kLonghun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_anomaly_immune(b);
        grant_gate(b, 100);
        run_cast(b, 0, 20261109);
        CHECK_EQ(a737(ctx, 0, 1), 2, "T9 行参 异常码=2（烧伤）");
        CHECK_EQ(a737(ctx, 0, 3), 0, "T9 未触发");
        CHECK_EQ(a737(ctx, 0, 6), 150000, "T9 核心：兜底量=最大体力1/4=150000（denom 变体）");
        std::cout << "  [T9] 737 denom 变体（龙魂焚火咒） 通过\n";
    }

    //===== T10 737 链中位切片：19454 [737,903][50,8,4,30] —— 737 吃前三、903 吃尾 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kFeihong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_anomaly_immune(b);
        grant_gate(b, 100);
        run_cast(b, 0, 20261109);
        CHECK_EQ(a737(ctx, 0, 0), 50, "T10 核心：行参 P=50（平面切片零串位——尾参 30 归 903）");
        CHECK_EQ(a737(ctx, 0, 2), 4, "T10 行参 分母=4");
        CHECK_EQ(a737(ctx, 0, 5), 1, "T10 兜底 fired");
        CHECK_EQ(a737(ctx, 0, 6), 150000, "T10 兜底量=150000");
        std::cout << "  [T10] 737 链中位切片（绯红破空杀） 通过\n";
    }

    //===== T11 737 击杀回合门：对手 1 血 + 掷败 → 攻击击杀 → 兜底 tick 未击败门跳过 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kZhuxian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                  /*speed=*/100, /*hp=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_gate(b, 0);
        run_cast(b, 0, 20261109);
        CHECK(b.pet(1).hp <= 0, "T11 前提：对手被攻击击杀");
        CHECK_EQ(a737(ctx, 0, 4), 1, "T11 兜底已 armed（SKILL_EFFECT 时点对手尚在场）");
        CHECK_EQ(a737(ctx, 0, 5), 0, "T11 核心：兜底未 fired（未击败门=J50 934 存活镜像）");
        CHECK_EQ(a737(ctx, 0, 6), 0, "T11 兜底量清账");
        std::cout << "  [T11] 737 击杀回合门（兜底跳过） 通过\n";
    }

    //===== T12 734 主正例：30267 [200] + 对手净槽 → 恢复 200 精确（半血观测）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kTasa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;   // 压当前体力（最大 400000 不动——坑集 §4），+200 不 clamp
        run_cast(b, 0, 20261109);
        CHECK_EQ(b.pet(0).hp, 1200, "T12 核心：恢复 200 精确（对手无还手，掉血=0）");
        CHECK_EQ(a734(ctx, 0, 0), 200, "T12 行参 n=200");
        CHECK_EQ(a734(ctx, 0, 1), 0, "T12 对手提升态种数=0");
        CHECK_EQ(a734(ctx, 0, 2), 0, "T12 对手下降态种数=0");
        CHECK_EQ(a734(ctx, 0, 3), 1, "T12 核心：门开（净槽）");
        CHECK_EQ(a734(ctx, 0, 4), 200, "T12 heal 实绩=200");
        CHECK_EQ(a734(ctx, 0, 8), 1, "T12 使用计数=1");
        std::cout << "  [T12] 734 门开恢复 200 精确 通过\n";
    }

    //===== T13 734 提升腿：对手攻+2 → 门闭、不恢复 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kTasa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        boost_foe(b, 0, 2);
        ctx.getPet(0).hp = 1000;
        run_cast(b, 0, 20261109);
        CHECK_EQ(b.pet(0).hp, 1000, "T13 核心：门闭不恢复（ hp 原样）");
        CHECK_EQ(a734(ctx, 0, 1), 1, "T13 对手提升态种数=1");
        CHECK_EQ(a734(ctx, 0, 3), 0, "T13 门闭");
        CHECK_EQ(a734(ctx, 0, 4), 0, "T13 heal 实绩=0（显式清账）");
        std::cout << "  [T13] 734 提升腿（门闭） 通过\n";
    }

    //===== T14 734 下降腿：对手速-1 → 门闭、不恢复（速槽降不翻先手：我方 300>对手 100）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kTasa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        drop_foe(b, 4, 1);
        ctx.getPet(0).hp = 1000;
        run_cast(b, 0, 20261109);
        CHECK_EQ(b.pet(0).hp, 1000, "T14 核心：门闭不恢复");
        CHECK_EQ(a734(ctx, 0, 2), 1, "T14 对手下降态种数=1");
        CHECK_EQ(a734(ctx, 0, 3), 0, "T14 门闭（两向净空判据——下降也闭门）");
        std::cout << "  [T14] 734 下降腿（门闭） 通过\n";
    }

    //===== T15 734 命中槽腿：对手命中+1（槽5） → 门闭（六槽口径含命中，K434 1325 同判）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kTasa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        boost_foe(b, 5, 1);
        ctx.getPet(0).hp = 1000;
        run_cast(b, 0, 20261109);
        CHECK_EQ(b.pet(0).hp, 1000, "T15 核心：命中槽提升也闭门（kAbilityLevelSlotCount=6）");
        CHECK_EQ(level(ctx, 1, 5), 1, "T15 对手命中槽=1");
        CHECK_EQ(a734(ctx, 0, 3), 0, "T15 门闭");
        std::cout << "  [T15] 734 命中槽腿（六槽口径） 通过\n";
    }

    //===== T16 734 变体量 + 混链载体：17129 [734,735][150] → 恢复 150（735 no-op 不扰）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kDimo, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        run_cast(b, 0, 20261109);
        CHECK_EQ(b.pet(0).hp, 1150, "T16 核心：恢复 150 精确（链上 735 未注册保持 no-op）");
        CHECK_EQ(a734(ctx, 0, 0), 150, "T16 行参 n=150（切片变体）");
        CHECK_EQ(a734(ctx, 0, 4), 150, "T16 heal 实绩=150");
        std::cout << "  [T16] 734 变体量+混链载体（地魔穿心箭） 通过\n";
    }

    std::cout << "scenario_8280 ALL PASS" << std::endl;
    return 0;
}
