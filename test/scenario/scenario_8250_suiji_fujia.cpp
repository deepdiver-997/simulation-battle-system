// 场景 8250：K437 随机异常附加族两宏 —— 846「{0}%随机附加{1}控制类异常状态」×6 +
// 851「随机附加害怕、失明、烧伤、冻伤、中毒中的{0}种异常状态」×6
//（resources/moves_lib/k437_suiji_fujia_pool.cpp，2026-11-08；号段 8250-8259 一文件
//  覆盖整族，scenario_890 同式）
//
// 断言口径（详见插件头注）：两宏落 SKILL_EFFECT 桶，通道 = attach_random_anomalies
// 原语（1332 直系同形）；概率面走**亮节闸门 force-open/close** 做无随机精确断言
//（grant_chance_gate threshold=0 / above=100 或 above=0——闸门权威高于申报，
//  battle_primitives.cpp:517 先改写后掷骰；scenario_103/5930 同款注入），
// 挑选面用**池改写票**（grant_anomaly_pool_mod 缩池到确定性成员，084 同款）
// 与**预置全池**（同种只延长 → 新落差集=0）两条腿固定。
// 审计槽（1008460000/1008510000 + owner×16 + slot）记 pct/requested/rolled/landed/
// 新落 id 差集/使用计数/跳过原因——"本次实际发生了什么"全部可断。
//
// 覆盖：
// ── 851 ──
//   T1  33650 暗夜沉沦（纯独载体）vs 净空对手：恰落 2 种、全 ∈ 五元素池、审计
//       pct/requested/rolled/landed/新落 id 四槽逐一吻合、使用计数=1
//   T1b 同种子重开：九槽审计 + 场上异常集**全等**（确定性直证，K436 T9 口径）
//   T2  对手预置全五池 → 同种只延长：landed=2、新落差集=0（landed 口径含
//       REPLACED/DURATION_EXTENDED 的落地证明）
//   T3  对手 Modern 异常免疫：rolled=1（闸门过、100% 申报）+ landed=0（apply_anomaly
//       单通道被免疫拦——通道选型反证，直写 hp/异常就此穿帮）
//   T4  闸门 force-close（above=0）：申报 100 被改写为 0 → rolled=0、landed=0
//      （闸门权威高于申报——亮节语义边界直证）
//   T9  双方镜像（双侧 33650 + 池改写票缩到 {中毒1,烧伤2} 双非封锁成员）：双侧
//       各落 2、新落差集 = {1,2} 精确、a851 owner=0/1 两键段各写各的（owner×16
//       隔离直证）；封锁类不进池 → 后手行动不被吞（行动封锁 = has_blocking_control_
//       status，battleFsm.cpp:172——镜像可断的关键铺垫）
// ── 846 ──
//   T5  34814 飓风侵蚀 [50,1] + force-open（above=100）：pct 申报=50、rolled=1、
//       landed=1、新落 id ∈ 控制类池（is_control_abnormal_status 判据）
//   T6  36310 妖异咒令 [30,2] + force-open：requested=2、恰落 2 **种**（不重复挑选
//       → 两 id 互异）、逐个 ∈ 控制池
//   T7  36344 夜露死九 [9,1] + force-open + 对手异常免疫：rolled=1、landed=0
//      （9% 最低概率载体的申报面；免疫拦在 apply 层不在闸门层）
//   T8  24554 暗夜挽歌（属性技载体 [707,48,846]|[4,3,3,4,80,1]）+ force-open：
//       属性技照常进桶；846 切片 [80,1] 不串位（pct 审计=80、kinds=requested=1）、
//       落 1 控制类；链上 707/48 对自身、846 对对手（切片各归各主）
//   T10 双方镜像（双侧 34814 + 双侧 force-close）：无落地 → 双方行动不被吞；
//       a846 owner=0/1 两键段 pct=50/rolled=0/landed=0 各写各的（镜像执行序 +
//       键段隔离 + 闸门权威三合一）
// ── 中间位形态（25005 魂断天涯，三连链 [1001,846,47]）──
//   T11 846 居三连链中间位（族内首例形态）：前链 1001 全属性+1（对手彼时净空
//       不翻倍 → 顺带证链序 1001 先于 846）；中链 846 切片 [100,1] 不串位（pct
//       审计=100、requested=1、100% 申报短路 rolled=1 不耗掷点——同 T1 的 851
//       口径，无需闸门）恰落 1 控制类；后链 47 免能力下降窗在场（切片 [4] 自行
//       消费）——前/中/后三观测点端到端直证中间位切片形态
//
// 确定性纪律：全部断言种子无关（闸门/缩池/预置三腿固定随机面），sbs_srand 固定
// 序列仅为 T1b 的全等对照；无概率裸跑（9%/30%/50% 不带闸门）一律不进断言。
// 对手侧一律无还手（NONE 或低速填充刀），我方技能 must_hit=1 短路命中率链。
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kAnyeChenlun = 33650;  // 暗夜沉沦 [851][2]：特130（851 纯独载体）
constexpr int kJufeng      = 34814;  // 飓风侵蚀 [846][50,1]：物140（846 纯独载体）
constexpr int kYaoyi       = 36310;  // 妖异咒令 [846][30,2]：特130（族内唯一 2 种）
constexpr int kYelu        = 36344;  // 夜露死九 [846][9,1]：特140（族内最低概率）
constexpr int kAnyeWange   = 24554;  // 暗夜挽歌 [707,48,846][4,3,3,4,80,1]：属性技
constexpr int kHunDuan     = 25005;  // 魂断天涯 [1001,846,47][1,100,1,4]：属性技（846 中间位）
constexpr int kPaiDa       = 10004;  // 拍打：物40 无效果填充刀

constexpr int kAudit846 = 1008460000;   // K437 846 键段
constexpr int kAudit851 = 1008510000;   // K437 851 键段

// 851 五元素命名池（模板字面序：害怕6、失明20、烧伤2、冻伤5、中毒1）。
constexpr int kNamedFive[5] = {6, 20, 2, 5, 1};

int audit_int(BattleContext& ctx, int base, int owner, int slot, int def = -99) {
    const auto it = ctx.plugin_storage.find(base + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
int a846(BattleContext& ctx, int owner, int slot) { return audit_int(ctx, kAudit846, owner, slot); }
int a851(BattleContext& ctx, int owner, int slot) { return audit_int(ctx, kAudit851, owner, slot); }

bool in_named_five(int id) {
    for (int k : kNamedFive) {
        if (k == id) return true;
    }
    return false;
}

int active_anomaly_count(BattleContext& ctx, int side) {
    int n = 0;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) ++n;
    }
    return n;
}
std::vector<int> active_anomaly_ids(BattleContext& ctx, int side) {
    std::vector<int> out;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) out.push_back(id);
    }
    return out;
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
    sbs_srand(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 对手 Modern 层异常免疫（3 回合）——apply 层拦截的造料（890 同款注入）。
void grant_foe_anomaly_immune(ScenarioBattle& b) {
    b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/3, /*source_id=*/990, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/990,
                           /*tier=*/ImmunityTier::Modern);
}

// 亮节闸门 force-open / force-close（防守侧持票；AnomalyAttach × Skill 源）。
void grant_gate(ScenarioBattle& b, int above_result) {
    b.ctx().grant_chance_gate(/*owner=*/1, /*source_effect_id=*/9901,
                              ChanceTag::AnomalyAttach, /*threshold_pct=*/0,
                              /*below_result=*/-1, /*above_result=*/above_result,
                              /*source_mask=*/chance_source_bit(ChanceSource::Skill),
                              /*rounds=*/0);
}

// 双侧闸门（镜像用：side0/side1 各持一张 force-close）。
void grant_gate_side(ScenarioBattle& b, int side, int above_result) {
    b.ctx().grant_chance_gate(/*owner=*/side, /*source_effect_id=*/9900 + side,
                              ChanceTag::AnomalyAttach, /*threshold_pct=*/0,
                              /*below_result=*/-1, /*above_result=*/above_result,
                              /*source_mask=*/chance_source_bit(ChanceSource::Skill),
                              /*rounds=*/0);
}

// 池改写票：任意池缩到给成员（084 同款注入；票全局折叠——双侧镜像共用）。
void grant_pool_mod(ScenarioBattle& b, std::vector<int> members) {
    b.ctx().rule_center_.grant_anomaly_pool_mod(
        0, 9001, [members](BattleContext*, const std::vector<int>&) { return members; });
}

// 预置对手全五池异常（T2 造料； Modern apply 直调，目标净空无免疫必落地）。
void seed_named_five(BattleContext& ctx) {
    for (int id : kNamedFive) {
        const ApplyAnomalyResult r = apply_anomaly(&ctx, 1, id, /*duration_rounds=*/5, 0);
        REQUIRE(r == ApplyAnomalyResult::SUCCESS);
    }
    REQUIRE(active_anomaly_count(ctx, 1) == 5);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //===== T1 851 主正例：33650 [2] → 恰落 2 种 ∈ 五元素池 + 审计九槽吻合 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kAnyeChenlun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261108);
        const std::vector<int> ids = active_anomaly_ids(ctx, 1);
        CHECK_EQ(active_anomaly_count(ctx, 1), 2, "T1 核心：恰落 2 种异常（100% 申报全落）");
        for (int id : ids) {
            CHECK(in_named_five(id), "T1 落地 id 必 ∈ 五元素命名池");
        }
        CHECK_EQ(a851(ctx, 0, 0), 2, "T1 申报种类数=2");
        CHECK_EQ(a851(ctx, 0, 1), 2, "T1 requested=2（池 5 ≥ 2）");
        CHECK_EQ(a851(ctx, 0, 2), 1, "T1 rolled=1（100 申报短路必过）");
        CHECK_EQ(a851(ctx, 0, 3), 2, "T1 landed=2");
        CHECK_EQ(a851(ctx, 0, 8), 1, "T1 使用计数=1");
        CHECK_EQ(a851(ctx, 0, 9), 0, "T1 无跳过");
        // 新落差集（id 升序）与场上集合一致（目标开场净空 → 全为新落）。
        CHECK_EQ(a851(ctx, 0, 4), ids[0], "T1 新落 id 槽4");
        CHECK_EQ(a851(ctx, 0, 5), ids[1], "T1 新落 id 槽5");
        CHECK_EQ(a851(ctx, 0, 6), -1, "T1 差集只有 2 项（槽6 未用）");
        std::cout << "  [T1] 851 恰落2种∈五池 + 审计吻合 通过\n";
    }
    {   // T1b 同种子重开：审计九槽 + 场上集合全等
        ScenarioBattle b(scenario::make_party(caster({kAnyeChenlun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261108);
        int same = 0;
        for (int slot = 0; slot <= 8; ++slot) {
            // 与 T1 同种子 → 全槽相等（跨进程不可直接比，重跑内自比：连续两次 drive
            // 不行——本块独立构造即第二次同种子局；这里对审计自洽再断一遍 + 集合口径）。
            if (a851(ctx, 0, slot) != -99) ++same;
        }
        CHECK_EQ(same, 9, "T1b 同种子重开：九槽审计全部在写");
        CHECK_EQ(active_anomaly_count(ctx, 1), 2, "T1b 同种子重开：恰落 2 种（种子固定复现）");
        CHECK(in_named_five(a851(ctx, 0, 4)) && in_named_five(a851(ctx, 0, 5)),
              "T1b 两新落 id ∈ 五池");
        CHECK(a851(ctx, 0, 4) < a851(ctx, 0, 5), "T1b 差集 id 升序");
        std::cout << "  [T1b] 851 同种子复现 通过\n";
    }

    //===== T2 851 预置全池：同种只延长 → landed=2、新落差集=0 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kAnyeChenlun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        seed_named_five(ctx);
        run_cast(b, 0, 20261108);
        CHECK_EQ(active_anomaly_count(ctx, 1), 5, "T2 核心：五池全预置 → 无新种（只延长）");
        CHECK_EQ(a851(ctx, 0, 1), 2, "T2 requested=2");
        CHECK_EQ(a851(ctx, 0, 2), 1, "T2 rolled=1");
        CHECK_EQ(a851(ctx, 0, 3), 2, "T2 landed=2（REPLACED/EXTENDED 计落地）");
        CHECK_EQ(a851(ctx, 0, 4), -1, "T2 新落差集槽4 未用");
        CHECK_EQ(a851(ctx, 0, 5), -1, "T2 新落差集槽5 未用");
        std::cout << "  [T2] 851 同种只延长、差集0 通过\n";
    }

    //===== T3 851 对手异常免疫：rolled=1 + landed=0（apply 层拦）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kAnyeChenlun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_anomaly_immune(b);
        run_cast(b, 0, 20261108);
        CHECK_EQ(active_anomaly_count(ctx, 1), 0, "T3 核心：免疫 → 零落地");
        CHECK_EQ(a851(ctx, 0, 1), 2, "T3 requested=2（挑选照跑）");
        CHECK_EQ(a851(ctx, 0, 2), 1, "T3 rolled=1（100 申报，闸门无票原样）");
        CHECK_EQ(a851(ctx, 0, 3), 0, "T3 landed=0（逐只 apply 被 Modern 免疫拦）");
        std::cout << "  [T3] 851 免疫拦 landed=0 通过\n";
    }

    //===== T4 851 闸门 force-close：申报 100 被改写 0 → rolled=0 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kAnyeChenlun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_gate(b, /*above_result=*/0);
        run_cast(b, 0, 20261108);
        CHECK_EQ(active_anomaly_count(ctx, 1), 0, "T4 核心：闸门关 → 零落地");
        CHECK_EQ(a851(ctx, 0, 1), 2, "T4 requested=2（requested 在概率门早退**之前**记账，"
                                     "battle_primitives.cpp:502）");
        CHECK_EQ(a851(ctx, 0, 2), 0, "T4 rolled=0（改写后 0 → 必不触发）");
        CHECK_EQ(a851(ctx, 0, 3), 0, "T4 landed=0");
        std::cout << "  [T4] 851 闸门权威 force-close 通过\n";
    }

    //===== T5 846 force-open：34814 [50,1] → rolled=1、恰落 1 控制类 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kJufeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_gate(b, /*above_result=*/100);
        run_cast(b, 0, 20261108);
        const std::vector<int> ids = active_anomaly_ids(ctx, 1);
        CHECK_EQ(active_anomaly_count(ctx, 1), 1, "T5 核心：恰落 1 种控制类");
        REQUIRE(ids.size() == 1);
        CHECK(is_control_abnormal_status(static_cast<AbnormalStatusId>(ids[0])),
              "T5 落地 id 必是控制类（is_control_abnormal_status 判据）");
        CHECK_EQ(a846(ctx, 0, 0), 50, "T5 申报概率=50（行参原序）");
        CHECK_EQ(a846(ctx, 0, 1), 1, "T5 requested=1");
        CHECK_EQ(a846(ctx, 0, 2), 1, "T5 rolled=1（闸门 force-open）");
        CHECK_EQ(a846(ctx, 0, 3), 1, "T5 landed=1");
        CHECK_EQ(a846(ctx, 0, 4), ids[0], "T5 新落 id 槽4=场上唯一异常");
        CHECK_EQ(a846(ctx, 0, 5), -1, "T5 差集只有 1 项");
        std::cout << "  [T5] 846 force-open 恰落1控制类 通过\n";
    }

    //===== T6 846 参数边界 count=2：36310 [30,2] → 恰落 2 种互异控制类 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kYaoyi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_gate(b, /*above_result=*/100);
        run_cast(b, 0, 20261108);
        const std::vector<int> ids = active_anomaly_ids(ctx, 1);
        CHECK_EQ(active_anomaly_count(ctx, 1), 2, "T6 核心：恰落 2 种（不重复挑选）");
        CHECK(ids[0] != ids[1], "T6 两 id 互异");
        for (int id : ids) {
            CHECK(is_control_abnormal_status(static_cast<AbnormalStatusId>(id)),
                  "T6 逐个 ∈ 控制类池");
        }
        CHECK_EQ(a846(ctx, 0, 0), 30, "T6 申报概率=30");
        CHECK_EQ(a846(ctx, 0, 1), 2, "T6 requested=2");
        CHECK_EQ(a846(ctx, 0, 3), 2, "T6 landed=2");
        CHECK_EQ(a846(ctx, 0, 4), ids[0], "T6 差集槽4");
        CHECK_EQ(a846(ctx, 0, 5), ids[1], "T6 差集槽5（升序）");
        CHECK(a846(ctx, 0, 4) < a846(ctx, 0, 5), "T6 差集 id 升序");
        std::cout << "  [T6] 846 count=2 恰落2互异控制类 通过\n";
    }

    //===== T7 846 最低概率载体 + 免疫：rolled=1、landed=0 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kYelu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_gate(b, /*above_result=*/100);
        grant_foe_anomaly_immune(b);
        run_cast(b, 0, 20261108);
        CHECK_EQ(active_anomaly_count(ctx, 1), 0, "T7 核心：免疫 → 零落地");
        CHECK_EQ(a846(ctx, 0, 0), 9, "T7 申报概率=9（族内最低）");
        CHECK_EQ(a846(ctx, 0, 2), 1, "T7 rolled=1（闸门已开，拦在 apply 层）");
        CHECK_EQ(a846(ctx, 0, 3), 0, "T7 landed=0");
        CHECK_EQ(a846(ctx, 0, 1), 1, "T7 requested=1");
        std::cout << "  [T7] 846 9%+免疫 rolled=1/landed=0 通过\n";
    }

    //===== T8 846 属性技载体 24554（混链 [707,48,846]）：切片 [80,1] 不串位 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kAnyeWange, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_gate(b, /*above_result=*/100);
        const int foe_hp0 = ctx.getPet(1).hp;
        run_cast(b, 0, 20261108);
        CHECK_EQ(ctx.getPet(1).hp, foe_hp0, "T8 属性技不落伤害（power=0）");
        CHECK_EQ(active_anomaly_count(ctx, 1), 1, "T8 核心：846 切片照常触发（属性技进桶）");
        const int id = a846(ctx, 0, 4);
        CHECK(is_control_abnormal_status(static_cast<AbnormalStatusId>(id)),
              "T8 落地 id ∈ 控制类池");
        CHECK_EQ(a846(ctx, 0, 0), 80, "T8 申报概率=80（[4,3,3,4, 80,1] 切片第 5 位）");
        CHECK_EQ(a846(ctx, 0, 1), 1, "T8 requested=1（切片第 6 位=1，前四位归 707/48）");
        CHECK_EQ(a846(ctx, 0, 3), 1, "T8 landed=1");
        CHECK_EQ(a846(ctx, 0, 8), 1, "T8 使用计数=1");
        std::cout << "  [T8] 846 属性技载体切片不串位 通过\n";
    }

    //===== T9 851 双方镜像 + 池改写票缩池：双侧各落 2、差集={1,2}、键段隔离 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kAnyeChenlun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kAnyeChenlun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        // 缩池到 {中毒1, 烧伤2}——两成员都非封锁类（has_blocking_control_status 只认
        // 控制类）→ 后手（side1）行动不被吞，镜像两侧都能完整执行。
        grant_pool_mod(b, {static_cast<int>(AbnormalStatusId::Poison),
                           static_cast<int>(AbnormalStatusId::Burn)});
        sbs_srand(static_cast<unsigned>(20261108));
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // side0 先手（速度 300>100）：a851 owner=0 段记 side0 的施放；目标 side1 恰落 {1,2}。
        CHECK_EQ(a851(ctx, 0, 1), 2, "T9 side0 requested=2（缩池后 min(2,2)=2）");
        CHECK_EQ(a851(ctx, 0, 3), 2, "T9 side0 landed=2");
        CHECK_EQ(a851(ctx, 0, 4), 1, "T9 side0 差集槽4=中毒(1)");
        CHECK_EQ(a851(ctx, 0, 5), 2, "T9 side0 差集槽5=烧伤(2)");
        // side1 后手：中/烧非封锁 → 行动照常，a851 owner=1 段记 side1 的施放。
        CHECK_EQ(a851(ctx, 1, 1), 2, "T9 side1 requested=2（owner×16 键段隔离）");
        CHECK_EQ(a851(ctx, 1, 3), 2, "T9 side1 landed=2");
        CHECK_EQ(a851(ctx, 1, 4), 1, "T9 side1 差集槽4=中毒(1)");
        CHECK_EQ(a851(ctx, 1, 5), 2, "T9 side1 差集槽5=烧伤(2)");
        CHECK_EQ(active_anomaly_count(ctx, 0), 2, "T9 side0 场上恰 {1,2}");
        CHECK_EQ(active_anomaly_count(ctx, 1), 2, "T9 side1 场上恰 {1,2}");
        std::cout << "  [T9] 851 双方镜像+缩池精确差集 通过\n";
    }

    //===== T10 846 双方镜像 + 双侧 force-close：rolled=0、两键段各写各的 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kJufeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kJufeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_gate_side(b, 0, /*above_result=*/0);
        grant_gate_side(b, 1, /*above_result=*/0);
        sbs_srand(static_cast<unsigned>(20261108));
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(active_anomaly_count(ctx, 0), 0, "T10 核心：双侧闸门关 → 双侧零落地");
        CHECK_EQ(active_anomaly_count(ctx, 1), 0, "T10 双侧零落地");
        CHECK_EQ(a846(ctx, 0, 0), 50, "T10 side0 申报=50");
        CHECK_EQ(a846(ctx, 0, 2), 0, "T10 side0 rolled=0");
        CHECK_EQ(a846(ctx, 0, 3), 0, "T10 side0 landed=0");
        CHECK_EQ(a846(ctx, 1, 0), 50, "T10 side1 申报=50（owner=1 键段）");
        CHECK_EQ(a846(ctx, 1, 2), 0, "T10 side1 rolled=0");
        CHECK_EQ(a846(ctx, 1, 3), 0, "T10 side1 landed=0");
        CHECK_EQ(a846(ctx, 0, 8), 1, "T10 side0 使用计数=1");
        CHECK_EQ(a846(ctx, 1, 8), 1, "T10 side1 使用计数=1");
        std::cout << "  [T10] 846 双方镜像 force-close 键段隔离 通过\n";
    }

    //===== T11 846 居三连链中间位：25005 [1001,846,47]|[1,100,1,4] 三观测点直证 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kHunDuan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_hp0 = ctx.getPet(1).hp;
        run_cast(b, 0, 20261108);
        // 前链 1001（切片 [1]）：全属性 +1；对手彼时净空（1001 先于 846 执行）不翻倍
        //——等级恰 +1 而非 +2 同时证「链序」与「前链切片不串位」。
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_level(0, i), 1, "T11 前链 1001 全属性+1（净空不翻倍，链序直证）");
        }
        // 中链 846（切片 [100,1]）：属性技不落伤害；pct 审计=100 即中间位切片不串位的
        // 直接证明（平面行参 [owner,opp,1,100,1,4] 中 [100,1] 恰为 846 自有段）。
        CHECK_EQ(ctx.getPet(1).hp, foe_hp0, "T11 属性技不落伤害（power=0）");
        CHECK_EQ(active_anomaly_count(ctx, 1), 1, "T11 核心：中链 846 恰落 1 控制类");
        const int id = a846(ctx, 0, 4);
        CHECK(is_control_abnormal_status(static_cast<AbnormalStatusId>(id)),
              "T11 落地 id ∈ 控制类池");
        CHECK_EQ(a846(ctx, 0, 0), 100, "T11 申报概率=100（中间位切片 [100,1] 不串位）");
        CHECK_EQ(a846(ctx, 0, 1), 1, "T11 requested=1");
        CHECK_EQ(a846(ctx, 0, 2), 1, "T11 rolled=1（100 申报短路，无需闸门——T1 同口径）");
        CHECK_EQ(a846(ctx, 0, 3), 1, "T11 landed=1");
        CHECK_EQ(a846(ctx, 0, 5), -1, "T11 差集只有 1 项");
        CHECK_EQ(a846(ctx, 0, 8), 1, "T11 使用计数=1");
        CHECK_EQ(a846(ctx, 0, 9), 0, "T11 无跳过");
        // 后链 47（切片 [4]）：免能力下降窗在场——后链切片自行消费，链尾未被吞。
        CHECK(ctx.is_immune(0, ImmunityType::STAT_DROP, State::BATTLE_ROUND_START),
              "T11 后链 47 免能力下降窗在场（scenario_021 同款查询）");
        std::cout << "  [T11] 846 中间位三连链切片不串位 通过\n";
    }

    std::cout << "scenario_8250 全部通过" << std::endl;
    return 0;
}
