// 场景 044：连击（"1回合做 x~y 次攻击"）—— **一次伤害公式 ×N**，不是 N 次独立结算。
//
// 官方口径（reference/scrape/out/all_posts_final.jsonl）：
//   · 变威力家族（L453）：「变威力效果的描述，通用特点就是技能威力提升n%，**n次连击**，
//     威力提升n点等」——连击与"改威力"**同族**。
//   · 公式（L101）：
//       [(等级×0.4+2)×面板威力×攻击÷防御÷50+2]×本系修正×克制系数
//          ×(217~255)÷255×暴击抗性×暴击系数×**连击次数**
//     「该公式计算均需向下取整，方括号内属于一个整体不可拆分」→ **一次公式 ×N**。
//   · 取整顺序（L99）：「浮动前一大段取整，浮动计算后取整、暴击系数计算后取整、
//     抗性计算后取整、**连击次数取整**」→ `×连击次数` 排在**最后**。
//   · 用户 2026-09-14 裁定把两条流水线合并：「先破防，然后再次按照单次暴击计算伤害，然后×N」。
//
// 引擎落点：`stage_simple_attack_damage` 里**暴击乘完之后**把 `base *= N`（N 由
// `resolve_skill_execution` 每次技能使用掷一次，写 `ws.combo_view[owner]`）。
// 乘在 `base` 上是刻意的——下游 deal_damage／伤害管线／日志全都只消费 final，
// 于是增伤/减伤/护盾/**次数型免伤**自动作用在**总数**上（"一次公式 ×N"的官方语义）。
// 且 **N>1 本身就是"变威力技能"** → 走 `apply_variable_power_recalc` 的第二次结算，
// 于是 `×N` 吃到的是**破防之后**的能力等级。
//
// 验证点：
//   C1 静态基数解析：从 side_effect 参数取下/上限（1172/1627 是 {1}{2}，其余是 {0}{1}）
//   C2 ×N 生效：白盒把连击数改成 3 → 伤害 ≈ 3× 对照
//   C3 端到端真技能（阴阳三合 5~10 次）→ 伤害落在"单段 ×[5,10]"区间
//   C4 ★ 连击也算变威力：必暴 + 对手防御 +2 时，N=3 的伤害 ≈ 6× N=1 的伤害
//      （3 倍来自连击、2 倍来自"第二次结算读到归零后的双防"）
//   C5 hitCount 落到快照（日志据此打印"N连击"）
//   C6 ★ 次数型免伤作用于**总数**：挂"免疫下 1 次攻击伤害"，N=3 的攻击**整段归零**
//      （而不是只挡第一段）——这是"一次公式 ×N"与"N 次独立结算"的分水岭
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kQuanLi = 10033;    // 全力一击：物理 power 120、无 side_effect（单段对照）
constexpr int kYinYang = 35375;   // 阴阳三合：特殊 power 15、effect 1454「1回合做5-10次攻击」
constexpr int kLangDa = 37381;    // 星光·浪打千击：物理 power 30、effect 1172「1回合做5~10次」
constexpr int kWanNian = 35526;   // 万念俱寂：effect 1627「{0}回合做{1}-{2}次」
constexpr int kLianHuan = 36895;  // 连环殴打：effect 1593「1回合做3-7次攻击」
constexpr int kGaoSu = 20017;     // 高速移动：属性技能（不还手）

constexpr int kDefenseStat = 2;   // levels[2] = 防御（物理攻击走它）

scenario::ScenarioBattle make_battle(int attacker_skill) {
    auto p0 = scenario::make_pet(0, "攻方", {attacker_skill, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/300, /*atk=*/400, /*hp=*/3000);
    auto p1 = scenario::make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/100, /*atk=*/400, /*hp=*/3000);
    p0.skills[0].accuracy = 100;
    p0.skills[0].must_hit = true;   // 排除 miss 干扰（miss 会阻止暴击）
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

// 打一回合：0 用技能 0 打 1，1 用属性技能不还手。
void strike(scenario::ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.drive();
}

//========== 探针 ==========
// 在 SKILL_EFFECT 时点把连击数改成指定值（模拟"连击数+{0}"类效果；写入即标记"变威力"）。
int g_combo_override = 0;
EffectResult set_combo_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    ctx->ws.combo_view[owner] = g_combo_override;
    return EffectResult::kOk;
}

// 在 ATTACK_DAMAGE 之后的时点读快照里的连击数（此时快照还没被清）。
int g_hit_count_probe = -1;
EffectResult probe_hit_count(BattleContext* ctx, const EffectArgs&) {
    if (ctx) {
        g_hit_count_probe = ctx->resolvedDamage.hitCount;
    }
    return EffectResult::kOk;
}

void register_effect(BattleContext& ctx, int owner, State state, EffectFn fn, int effect_id) {
    Effect e(effect_id, /*priority=*/0, owner, /*left_round=*/1,
             EffectArgs(std::vector<int>{owner, 1 - owner}), fn);
    auto ce = std::make_unique<ContinuousEffect>(e, state, owner, /*duration=*/1, ctx.roundCount);
    ce->source_id_ = effect_id;
    ctx.register_skill_effect(state, owner, std::move(ce));
}

void register_combo_override(BattleContext& ctx, int owner, int combo) {
    g_combo_override = combo;
    register_effect(ctx, owner, State::BATTLE_FIRST_SKILL_EFFECT, &set_combo_effect, 999044);
}

// 给守方摆上 +2 防御（本体 + 视图都要写：伤害公式读 ws.view_levels，破防写两边）。
void give_defense_boost(BattleContext& ctx, int level) {
    ctx.getPet(1).levels[kDefenseStat] = level;
    ctx.ws.view_levels[1][kDefenseStat] = level;
}

// 给守方(1)授予"免疫下 counts 次受到的攻击伤害"（覆盖两个攻击伤害时点）。
void grant_count_immune(BattleContext& ctx, int counts) {
    const uint64_t coverage = coverage_union(
        {State::BATTLE_FIRST_ATTACK_DAMAGE, State::BATTLE_SECOND_ATTACK_DAMAGE});
    ctx.grant_immunity(
        /*owner=*/1, ImmunityType::DAMAGE, coverage,
        /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/0,
        /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/counts);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== C1 静态基数解析：从 side_effect 参数取"下/上限" ==========
    {
        Skills yy(kYinYang);    // 1454：args[0]=5, args[1]=10（"1回合做{0}-{1}次"）
        Skills ld(kLangDa);     // 1172：args[1]=5, args[2]=10（"{0}回合做{1}~{2}次"）
        Skills wn(kWanNian);    // 1627：args[1]=1, args[2]=10（同上，{0}回合开头）
        Skills lh(kLianHuan);   // 1593：args[0]=3, args[1]=7
        Skills plain(kQuanLi);  // 无连击模板 → 1~1

        CHECK_EQ(yy.combo_min, 5, "C1) 阴阳三合 1454：参数 {0}-{1} → 5~10");
        CHECK_EQ(yy.combo_max, 10, "C1) 阴阳三合 上限 10");
        CHECK_EQ(ld.combo_min, 5, "C1) 浪打千击 1172：参数 {1}~{2}（前面有回合数）→ 5~10");
        CHECK_EQ(ld.combo_max, 10, "C1) 浪打千击 上限 10");
        CHECK_EQ(wn.combo_min, 1, "C1) 万念俱寂 1627：同为 {0}回合开头 → 取 {1}-{2} → 1~10");
        CHECK_EQ(wn.combo_max, 10, "C1) 万念俱寂 上限 10");
        CHECK_EQ(lh.combo_min, 3, "C1) 连环殴打 1593 → 3~7");
        CHECK_EQ(lh.combo_max, 7, "C1) 连环殴打 上限 7");
        CHECK_EQ(plain.combo_min, 1, "C1) 无连击模板 → 下限 1");
        CHECK_EQ(plain.combo_max, 1, "C1) 无连击模板 → 上限 1（区间退化，掷点不消耗 rand）");
        std::cout << "  [C1] 静态基数: 阴阳三合=" << yy.combo_min << "~" << yy.combo_max
                  << "、浪打千击=" << ld.combo_min << "~" << ld.combo_max
                  << "、万念俱寂=" << wn.combo_min << "~" << wn.combo_max
                  << "、连环殴打=" << lh.combo_min << "~" << lh.combo_max
                  << "、全力一击=" << plain.combo_min << "~" << plain.combo_max << "\n";
    }

    //========== C2 ×N 生效：连击数 3 → 伤害 ≈ 3× ==========
    int dmg_single = 0;
    {
        scenario::ScenarioBattle b = make_battle(kQuanLi);
        b.start();
        const int hp = b.ctx().getPet(1).hp;
        strike(b);
        dmg_single = hp - b.ctx().getPet(1).hp;

        scenario::ScenarioBattle b3 = make_battle(kQuanLi);
        b3.start();
        register_combo_override(b3.ctx(), 0, /*combo=*/3);
        const int hp3 = b3.ctx().getPet(1).hp;
        strike(b3);
        const int dmg3 = hp3 - b3.ctx().getPet(1).hp;

        // 理论比值 3.0；两次结算各自掷 217~255 浮动 → 下界 3*0.851 ≈ 2.55，取 2.0 留余量。
        CHECK(dmg3 > dmg_single * 2, "C2) 连击数 3 → 伤害 ≈ 3× 单段");
        std::cout << "  [C2] ×N 生效: N=3 伤害=" << dmg3 << " vs 单段=" << dmg_single << "\n";
    }

    //========== C3 端到端真技能：阴阳三合（5~10 次）==========
    {
        scenario::ScenarioBattle b = make_battle(kYinYang);
        b.start();
        const int hp = b.ctx().getPet(1).hp;
        strike(b);
        const int dmg = hp - b.ctx().getPet(1).hp;

        // 单段理论值 = 0.84*400/100*15 + 2 = 52.4；×浮动[0.851,1.0] → [44.6, 52.4]
        // → ×[5,10] 次 → 理论区间 [223, 524]。放宽到 [180, 600] 以容纳取整与将来微调。
        CHECK(dmg > 180, "C3) 多段：伤害远高于单段（单段只有 ~50）");
        CHECK(dmg < 600, "C3) 多段：伤害不超过 10 段的上界");
        std::cout << "  [C3] 阴阳三合(5~10次) 端到端: 伤害=" << dmg
                  << "（单段约 45~52，5~10 段 → 约 223~524）\n";
    }

    //========== C4 ★ 连击也算变威力：N=3 的伤害 ≈ 6× N=1 ==========
    // 对手防御 +2、我方必暴。N=1 时本次伤害在破防之前就算完（×1），
    // N=3 时 N>1 触发第二次结算 → 吃到归零后的双防（防御 200→100，伤害 ×2）再 ×3 → ≈6×。
    // 若引擎不把"多段"当变威力（只 ×3 不重算），比值会停在 ≈3 → 本断言即失败。
    {
        scenario::ScenarioBattle b1 = make_battle(kQuanLi);
        scenario::set_crit_sixteenths(b1.ctx().getPet(0), 16);   // 必定暴击
        b1.start();
        give_defense_boost(b1.ctx(), 2);
        const int hp1 = b1.ctx().getPet(1).hp;
        strike(b1);
        const int dmg1 = hp1 - b1.ctx().getPet(1).hp;

        scenario::ScenarioBattle b3 = make_battle(kQuanLi);
        scenario::set_crit_sixteenths(b3.ctx().getPet(0), 16);
        b3.start();
        give_defense_boost(b3.ctx(), 2);
        register_combo_override(b3.ctx(), 0, /*combo=*/3);
        const int hp3 = b3.ctx().getPet(1).hp;
        strike(b3);
        const int dmg3 = hp3 - b3.ctx().getPet(1).hp;

        // 理论 3 × 2 = 6；下界 = 3 × (405.2*0.851)/(203.6*1.0) ≈ 5.08，取 4 留余量。
        CHECK(dmg3 > dmg1 * 4,
              "C4) ★ 连击走第二次结算 → ×3 连击 ×2 破防 ≈ 6× 单段（若只 ×3 则比值≈3）");
        std::cout << "  [C4] 连击+暴击破防: N=3=" << dmg3 << " vs N=1=" << dmg1
                  << "（比值 " << (dmg1 > 0 ? dmg3 * 100 / dmg1 : 0) << "%，理论 600%）\n";
    }

    //========== C5 hitCount 落到快照（日志据此打印"N连击"）==========
    {
        scenario::ScenarioBattle b = make_battle(kYinYang);
        b.start();
        BattleContext& ctx = b.ctx();
        g_hit_count_probe = -1;
        register_effect(ctx, 0, State::BATTLE_FIRST_AFTER_ACTION, &probe_hit_count, 999045);
        strike(b);
        CHECK(g_hit_count_probe >= 5 && g_hit_count_probe <= 10,
              "C5) 快照 hitCount 落在 [5,10]（阴阳三合 5~10 次）");
        std::cout << "  [C5] 快照 hitCount=" << g_hit_count_probe << "（日志会打印\""
                  << g_hit_count_probe << "连击\"）\n";
    }

    //========== C6 ★ 次数型免伤作用于总数 ==========
    {
        scenario::ScenarioBattle b = make_battle(kQuanLi);
        b.start();
        grant_count_immune(b.ctx(), /*counts=*/1);
        register_combo_override(b.ctx(), 0, /*combo=*/3);
        const int hp = b.ctx().getPet(1).hp;
        strike(b);
        const int dmg = hp - b.ctx().getPet(1).hp;
        CHECK_EQ(dmg, 0,
                 "C6) ★ \"免疫下1次攻击伤害\"吞掉**整段**（不是只挡 1 段中的第一段）"
                 "——一次公式 ×N 与 N 次独立结算的分水岭");
        std::cout << "  [C6] 次数型免伤 + N=3: 伤害=" << dmg
                  << "（整段被吞；若按 N 次独立结算则只会挡掉 1/3）\n";
    }

    std::cout << "\n场景 044 通过：连击 = **一次伤害公式 ×N**（不是 N 次独立结算）；"
                 "×N 排在公式最后（暴击之后），乘在 base 上 → 增伤/减伤/护盾/次数型免伤"
                 "全部作用于总数；N>1 本身即变威力 → 走第二次结算，吃到破防后的能力等级。\n";
    return 0;
}
