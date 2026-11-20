// 场景 8260：K438 排除清单随机族重审三宏 —— 1116「随机附加{0}种异常状态，未触发则
// 消除对手回合类效果」×6 + 1624「对手不处于异常状态时随机附加{0}种异常状态」×5 +
// 2074「随机附加{0}种非限制类异常状态」×6
//（resources/moves_lib/k438_rand_attach_recheck.cpp，2026-11-09；号段 8260-8269
//  一文件覆盖三族，scenario_8250 同式）
//
// 断言口径（详见插件头注）：三宏落 SKILL_EFFECT 桶，通道 = attach_random_anomalies
// 原语（1116+2074 = 846/851/1332 直系同形；1624 = 原语 + 1339 条件门拼装）。
// 无概率子句三宏全部申报 100（原语内 >=100 短路）；随机面固定三腿：
//   ① 池改写票缩池到确定性成员（grant_anomaly_pool_mod，084/8250 同款）；
//   ② 池改写**恒等观测票**（lambda 原样返回入参池、把池大小/边界成员记进
//      plugin_storage——2074 非限制池边界的种子无关直证）；
//   ③ 预置异常/Modern 免疫造料（1624 门开闭两态、1116 landed==0 补偿路径）。
// 断言种子无关，无随机裸跑。
//
// 覆盖：
// ── 2074（非限制池）──
//   T1  37491 践战冲锋（纯独载体 [1]）+ 缩池 {2}：恰落 1 种=id 2、审计十槽逐一吻合
//   T2  37654 浊浪寥寥（纯独载体 [1]）+ 恒等观测票：池大小=42（44⊖瘫痪19/凝滞32）、
//       19/32 不在池、含边界 0 与 43——非限制池边界种子无关直证（池实现变异必红）
//   T3  37445 红艳宴（链 [143,2074]、143 零参切片 []）+ 缩池 {1,2}：恰落 2 种互异
//       ={1,2}（不重复挑选直证）、前链零参切片不串位
// ── 1624（条件门）──
//   T4  28134 灵光之束（纯独载体 [1]，属性技载体照常进桶）+ 对手净空 + 缩池 {2}：
//       门=1、恰落 id 2、审计吻合
//   T5  28134 + 对手预置烧伤：门=0、不附加（requested/rolled/landed 显式 0、场上
//       仅预置那 1 条）、使用计数照加
//   T6  34616 妖精的蛊惑（链 [1589,1624] 纯链）+ 对手 Modern 异常免疫（无活跃异常）：
//       门=1（免疫≠处于）、rolled=1、landed=0（apply 层拦）——门判据语义直证
//   T7  34995 邪魅鬼影门（自带 must_hit、链 [1624,487,488] 尾位两兄弟各吃各参）
//       + 缩池 {1}：恰落 id 1、混链切片不串位
// ── 1116（未触发补偿 = 消回合）──
//   T8  32299 恐惧之缚（链 [1116,857]）+ 对手持可断票（890 同款 seal_skill 直调）
//       + 缩池 {1,2}：恰落 2 → **补偿不尝试**（槽10=0）、可断票存活
//   T9  32299 + 对手 Modern 异常免疫 + 可断票：landed=0 → 补偿 break SUCCESS
//       （槽10=1）、可断票被清（has_round_type(1) 翻假）
//   T10 双方镜像 32299 + 双侧净空 + 缩池 {1,2}：双侧各恰落 2、a1116 owner=0/1 两
//       键段各写各的（owner×16 隔离直证）
//   T11 32299 + 亮节闸门 force-close（above=0 把申报 100 改写为 0）：rolled=0、
//       landed=0 → 补偿照发（"未触发"含闸门改写零落——分支同源）、对手无可断物 →
//       NO_EFFECTS（槽10=2）
//
// 确定性纪律：全部断言种子无关（缩池/恒等观测票/预置造料三腿固定随机面），
// sbs_srand 固定序列仅为复现基座；无概率裸跑。对手侧一律无还手（NONE 或低速
// 填充刀），我方技能 must_hit=1 短路命中率链。
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

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kJianzhan   = 37491;  // 践战冲锋 [2074][1]：物130（2074 纯独载体）
constexpr int kZhuolang   = 37654;  // 浊浪寥寥 [2074][1]：特140（2074 纯独载体）
constexpr int kHongyan    = 37445;  // 红艳宴 [143,2074][ ,2]：特85（2074 混链，143 零参）
constexpr int kLingguang  = 28134;  // 灵光之束 [1624][1]：属性技（1624 纯独载体）
constexpr int kYaogu      = 34616;  // 妖精的蛊惑 [1589,1624][1,1]：特150（1624 纯链）
constexpr int kGuimo      = 34995;  // 邪魅鬼影门 [1624,487,488][1,500,2,500,50]：物150 必中
constexpr int kKongbu     = 32299;  // 恐惧之缚 [1116,857][2,50,1,100,1]：特150（1116 载体）
constexpr int kPaiDa      = 10004;  // 拍打：物40 无效果填充刀

constexpr int kAudit1116 = 1011160000;   // K438 1116 键段
constexpr int kAudit1624 = 1016240000;   // K438 1624 键段
constexpr int kAudit2074 = 1020740000;   // K438 2074 键段
constexpr int kPoolObs   = 2092074000;   // T2 恒等观测票落点（场景私有 2.09B 段）

int audit_int(BattleContext& ctx, int base, int owner, int slot, int def = -99) {
    const auto it = ctx.plugin_storage.find(base + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
int a1116(BattleContext& ctx, int owner, int slot) { return audit_int(ctx, kAudit1116, owner, slot); }
int a1624(BattleContext& ctx, int owner, int slot) { return audit_int(ctx, kAudit1624, owner, slot); }
int a2074(BattleContext& ctx, int owner, int slot) { return audit_int(ctx, kAudit2074, owner, slot); }

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

// R1 骨架：我方出载体，对手无还手（镜像用 cast_mirror：双侧各选 0 号）。
void run_cast(ScenarioBattle& b, int skill_index, int seed) {
    sbs_srand(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}
void run_cast_mirror(ScenarioBattle& b, int seed) {
    sbs_srand(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// 对手 Modern 层异常免疫（3 回合）——landed=0 的造料（890/8250 同款注入）。
void grant_foe_anomaly_immune(ScenarioBattle& b) {
    b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/3, /*source_id=*/990, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/990,
                           /*tier=*/ImmunityTier::Modern);
}

// 亮节闸门 force-close（防守侧持票；AnomalyAttach × Skill 源；8250 同款注入）。
void grant_gate_force_close(ScenarioBattle& b) {
    b.ctx().grant_chance_gate(/*owner=*/1, /*source_effect_id=*/9901,
                              ChanceTag::AnomalyAttach, /*threshold_pct=*/0,
                              /*below_result=*/-1, /*above_result=*/0,
                              /*source_mask=*/chance_source_bit(ChanceSource::Skill),
                              /*rounds=*/0);
}

// 池改写票：任意池缩到给成员（084/8250 同款注入；票全局折叠）。
void grant_pool_mod(ScenarioBattle& b, std::vector<int> members) {
    b.ctx().rule_center_.grant_anomaly_pool_mod(
        0, 9001, [members](BattleContext*, const std::vector<int>&) { return members; });
}

// 池改写**恒等观测票**：原样返回入参池，把池大小/边界成员在场性记进 plugin_storage
// ——插件内非限制池边界的种子无关直证（T2 专用；键段 kPoolObs，owner=0）。
void grant_pool_observer(ScenarioBattle& b) {
    b.ctx().rule_center_.grant_anomaly_pool_mod(
        0, 9002, [](BattleContext* c, const std::vector<int>& pool) {
            const int key0 = 2092074000;
            c->plugin_storage[key0 + 0] = static_cast<int>(pool.size());
            c->plugin_storage[key0 + 1] =
                std::find(pool.begin(), pool.end(), 19) != pool.end() ? 1 : 0;
            c->plugin_storage[key0 + 2] =
                std::find(pool.begin(), pool.end(), 32) != pool.end() ? 1 : 0;
            c->plugin_storage[key0 + 3] = pool.empty() ? -1 : pool.front();
            c->plugin_storage[key0 + 4] = pool.empty() ? -1 : pool.back();
            return pool;
        });
}

// 预置对手烧伤（1624 门闭造料；Modern apply 直调，目标净空无免疫必落地）。
void seed_foe_burn(BattleContext& ctx) {
    const ApplyAnomalyResult r = apply_anomaly(&ctx, 1, /*burn=*/2, /*duration_rounds=*/5, 0);
    REQUIRE(r == ApplyAnomalyResult::SUCCESS);
    REQUIRE(active_anomaly_count(ctx, 1) == 1);
}

// 给 side 名下挂一条回合型封属票作"可断物"（890 同款注入，3 回合窗口）。
void grant_round_ticket(ScenarioBattle& b, int side) {
    seal_skill(&b.ctx(), /*source=*/side, /*target=*/1 - side, /*effect_id=*/70471,
               /*attribute=*/true, /*attack=*/false, /*count=*/0,
               /*duration_rounds=*/3, /*penetrable=*/true, /*source_slot=*/-1,
               EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
               /*consumed_when_pierced=*/false);
    CHECK(b.ctx().rule_center_.has_round_type(side), "前提：该侧持有回合类效果");
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //===== T1 2074 主正例：37491 [1] + 缩池 {2} → 恰落 id2 + 审计十槽吻合 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kJianzhan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_pool_mod(b, {2});
        run_cast(b, 0, 20261109);
        CHECK_EQ(active_anomaly_count(ctx, 1), 1, "T1 核心：恰落 1 种");
        CHECK_EQ(active_anomaly_ids(ctx, 1)[0], 2, "T1 落地 id=2（缩池唯一成员）");
        CHECK_EQ(a2074(ctx, 0, 0), 1, "T1 申报种类数=1");
        CHECK_EQ(a2074(ctx, 0, 1), 1, "T1 requested=1（池 1 ≥ 1）");
        CHECK_EQ(a2074(ctx, 0, 2), 1, "T1 rolled=1（100 申报短路必过）");
        CHECK_EQ(a2074(ctx, 0, 3), 1, "T1 landed=1");
        CHECK_EQ(a2074(ctx, 0, 4), 2, "T1 新落 id 槽4");
        CHECK_EQ(a2074(ctx, 0, 5), -1, "T1 差集只有 1 项（槽5 未用）");
        CHECK_EQ(a2074(ctx, 0, 8), 1, "T1 使用计数=1");
        CHECK_EQ(a2074(ctx, 0, 9), 0, "T1 无跳过");
        std::cout << "  [T1] 2074 缩池恰落1种 + 审计吻合 通过\n";
    }

    //===== T2 2074 非限制池边界：恒等观测票（种子无关直证）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kZhuolang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_pool_observer(b);
        run_cast(b, 0, 777);
        const int key0 = 2092074000;
        const auto get = [&](int slot) {
            const auto it = ctx.plugin_storage.find(key0 + slot);
            if (it == ctx.plugin_storage.end()) return -99;
            const int* v = std::any_cast<int>(&it->second);
            return v ? *v : -99;
        };
        CHECK_EQ(get(0), 42, "T2 核心：非限制池大小=42（44 全池 ⊖ 瘫痪19/凝滞32）");
        CHECK_EQ(get(1), 0, "T2 瘫痪19 不在池");
        CHECK_EQ(get(2), 0, "T2 凝滞32 不在池");
        CHECK_EQ(get(3), 0, "T2 池含边界小端 0（麻痹）");
        CHECK_EQ(get(4), 43, "T2 池含边界大端 43");
        CHECK_EQ(a2074(ctx, 0, 3), 1, "T2 landed=1（原池照常附加）");
        const std::vector<int> ids = active_anomaly_ids(ctx, 1);
        CHECK_EQ(static_cast<int>(ids.size()), 1, "T2 恰落 1 种");
        CHECK(ids[0] != 19 && ids[0] != 32, "T2 落地 id 必非限制类（池边界保证，种子无关）");
        std::cout << "  [T2] 2074 池边界观测（42⊖19/32） 通过\n";
    }

    //===== T3 2074 两种载体：37445 [143,2074] + 缩池 {1,2} → 恰落 2 种互异 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kHongyan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_pool_mod(b, {1, 2});
        run_cast(b, 0, 20261109);
        const std::vector<int> ids = active_anomaly_ids(ctx, 1);
        CHECK_EQ(static_cast<int>(ids.size()), 2, "T3 核心：恰落 2 种");
        CHECK_EQ(ids[0], 1, "T3 落地含中毒1（缩池两成员全落，id 升序）");
        CHECK_EQ(ids[1], 2, "T3 落地含烧伤2");
        CHECK_EQ(a2074(ctx, 0, 0), 2, "T3 申报种类数=2（行参切片 [2] 不串位）");
        CHECK_EQ(a2074(ctx, 0, 1), 2, "T3 requested=2");
        CHECK_EQ(a2074(ctx, 0, 3), 2, "T3 landed=2");
        CHECK_EQ(a2074(ctx, 0, 4), 1, "T3 新落差集槽4=1");
        CHECK_EQ(a2074(ctx, 0, 5), 2, "T3 新落差集槽5=2");
        CHECK_EQ(a2074(ctx, 0, 6), -1, "T3 差集恰 2 项");
        // 前链 143（零参切片 []，对手能力提升反转——对手净空 → no-op）不扰断言面。
        std::cout << "  [T3] 2074 恰落2种互异 + 零参前链不串位 通过\n";
    }

    //===== T4 1624 门开：28134（属性技载体）+ 净空 + 缩池 {2} → 恰落 id2 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kLingguang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_pool_mod(b, {2});
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1624(ctx, 0, 10), 1, "T4 门=1（对手净空）");
        CHECK_EQ(active_anomaly_count(ctx, 1), 1, "T4 核心：恰落 1 种");
        CHECK_EQ(active_anomaly_ids(ctx, 1)[0], 2, "T4 落地 id=2");
        CHECK_EQ(a1624(ctx, 0, 0), 1, "T4 申报种类数=1");
        CHECK_EQ(a1624(ctx, 0, 3), 1, "T4 landed=1");
        CHECK_EQ(a1624(ctx, 0, 8), 1, "T4 使用计数=1");
        // 属性技载体照常进 SKILL_EFFECT 桶（8250 T8 口径）。
        std::cout << "  [T4] 1624 门开恰落1种（属性技载体） 通过\n";
    }

    //===== T5 1624 门闭：对手预置烧伤 → 不附加 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kLingguang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        seed_foe_burn(ctx);
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1624(ctx, 0, 10), 0, "T5 核心：门=0（对手处于异常状态）");
        CHECK_EQ(active_anomaly_count(ctx, 1), 1, "T5 场上仍只有预置那 1 条");
        CHECK_EQ(active_anomaly_ids(ctx, 1)[0], 2, "T5 预置的烧伤 2 未被动过（未附加未延长）");
        CHECK_EQ(a1624(ctx, 0, 1), 0, "T5 requested 显式 0");
        CHECK_EQ(a1624(ctx, 0, 2), 0, "T5 rolled 显式 0");
        CHECK_EQ(a1624(ctx, 0, 3), 0, "T5 landed 显式 0");
        CHECK_EQ(a1624(ctx, 0, 8), 1, "T5 使用计数照加（宏跑过、门没开）");
        std::cout << "  [T5] 1624 门闭不附加 通过\n";
    }

    //===== T6 1624 免疫≠处于：34616 + 对手 Modern 异常免疫 → 门开但 landed=0 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kYaogu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_anomaly_immune(b);
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1624(ctx, 0, 10), 1, "T6 核心：门=1（免疫≠处于——无活跃异常即门开）");
        CHECK_EQ(a1624(ctx, 0, 2), 1, "T6 rolled=1（100 申报短路）");
        CHECK_EQ(a1624(ctx, 0, 3), 0, "T6 landed=0（逐只 apply 被 Modern 免疫拦）");
        CHECK_EQ(active_anomaly_count(ctx, 1), 0, "T6 对手零异常");
        std::cout << "  [T6] 1624 免疫≠处于门判据语义 通过\n";
    }

    //===== T7 1624 混链切片：34995（必中物150，尾位 487/488 各吃各参）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kGuimo, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_pool_mod(b, {1});
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1624(ctx, 0, 10), 1, "T7 门=1（对手净空）");
        CHECK_EQ(active_anomaly_count(ctx, 1), 1, "T7 核心：恰落 1 种");
        CHECK_EQ(active_anomaly_ids(ctx, 1)[0], 1, "T7 落地 id=1");
        CHECK_EQ(a1624(ctx, 0, 0), 1, "T7 申报种类数=1（头位切片 [1] 不串位）");
        CHECK_EQ(a1624(ctx, 0, 3), 1, "T7 landed=1");
        // 尾位 487（对手 HP 600000 > 500 → 自身攻击+2，无害）与 488（对手 HP < 500
        // 不成立 → no-op）各吃各参，不扰 1624 断言面。
        std::cout << "  [T7] 1624 三连链切片不串位 通过\n";
    }

    //===== T8 1116 落地档：恰落 2 → 补偿不尝试、可断票存活 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kKongbu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_pool_mod(b, {1, 2});
        grant_round_ticket(b, 1);
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1116(ctx, 0, 3), 2, "T8 核心：landed=2（缩池两成员全落）");
        CHECK_EQ(a1116(ctx, 0, 10), 0, "T8 补偿未尝试（槽10=0）");
        CHECK(ctx.rule_center_.has_round_type(1), "T8 可断票存活（触发档不消回合）");
        std::cout << "  [T8] 1116 触发档不消回合 通过\n";
    }

    //===== T9 1116 未触发档：对手免疫 → landed=0 → 补偿 break SUCCESS 清票 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kKongbu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_anomaly_immune(b);
        grant_round_ticket(b, 1);
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1116(ctx, 0, 2), 1, "T9 rolled=1（申报 100 照过）");
        CHECK_EQ(a1116(ctx, 0, 3), 0, "T9 核心：landed=0（免疫拦）");
        CHECK_EQ(a1116(ctx, 0, 10), 1, "T9 补偿 break=SUCCESS（槽10=1）");
        CHECK(!ctx.rule_center_.has_round_type(1), "T9 可断票被清（补偿真实生效）");
        std::cout << "  [T9] 1116 未触发档补偿消回合 通过\n";
    }

    //===== T10 1116 双方镜像：双侧各恰落 2 + owner 键段隔离 =====
    {
        ScenarioBattle b(scenario::make_party(caster({kKongbu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kKongbu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_pool_mod(b, {1, 2});
        run_cast_mirror(b, 20261109);
        CHECK_EQ(a1116(ctx, 0, 3), 2, "T10 owner0 landed=2");
        CHECK_EQ(a1116(ctx, 1, 3), 2, "T10 owner1 landed=2");
        CHECK_EQ(a1116(ctx, 0, 10), 0, "T10 owner0 补偿未尝试");
        CHECK_EQ(a1116(ctx, 1, 10), 0, "T10 owner1 补偿未尝试（双侧均无票可断）");
        CHECK_EQ(active_anomaly_count(ctx, 0), 2, "T10 侧0 场上恰 2 条");
        CHECK_EQ(active_anomaly_count(ctx, 1), 2, "T10 侧1 场上恰 2 条");
        CHECK_EQ(a1116(ctx, 0, 8), 1, "T10 owner0 使用计数=1（各写各的）");
        CHECK_EQ(a1116(ctx, 1, 8), 1, "T10 owner1 使用计数=1");
        std::cout << "  [T10] 1116 镜像 + 键段隔离 通过\n";
    }

    //===== T11 1116 闸门改写零落：force-close → landed=0 → 补偿照发（无可断物=NO_EFFECTS）=====
    {
        ScenarioBattle b(scenario::make_party(caster({kKongbu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_gate_force_close(b);
        run_cast(b, 0, 20261109);
        CHECK_EQ(a1116(ctx, 0, 1), 2, "T11 requested=2（挑选申报面不受闸门影响——原语先闸后掷）");
        CHECK_EQ(a1116(ctx, 0, 2), 0, "T11 核心：rolled=0（闸门把 100 改写为 0）");
        CHECK_EQ(a1116(ctx, 0, 3), 0, "T11 landed=0（掷点未过）");
        CHECK_EQ(a1116(ctx, 0, 10), 2, "T11 补偿 break=NO_EFFECTS（对手无可断物）");
        std::cout << "  [T11] 1116 闸门改写零落仍补偿 通过\n";
    }

    std::cout << "scenario_8260: all checks passed" << std::endl;
    return 0;
}
