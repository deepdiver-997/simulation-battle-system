// 场景 078 —— **属性伤害**（`deal_attribute_damage`；《赛学必修16—伤害类型》）
//
// 官方口径（`reference/scrape/out/all_posts.jsonl` idx=21）：
//   「**属性伤害**：由属性技能造成的技能伤害…通常会于效果中直接声明造成伤害的**系别与点数**，
//     如"附加 300 点电系伤害"…**往往是固定的数值乘以克制倍数**，若未提及则不会涉及攻防值」
//
// 它吃哪些修正（用户 2026-09-18 口径，本场景逐条锁）：
//   · 数值 = 声明点数 × **克制倍数**            → A/B/C/D 组
//   · **以红伤形式结算**（护盾响应、算红伤落血）  → E 组
//   · 受**锁伤**修正影响                        → F 组
//   · **常规挡伤不触发**（"免疫下N次攻击伤害"票） → G 组
//   · **不吃**攻击伤害的增/减伤                 → H 组
//
// 被测系别 = **次元·龙**（组件 {17次元, 15龙}，`skill_types` 120 = ["dimension","dragon"]）。
// 官方克制表（`types_relation`）实测取到（倍率聚合口径 **J322 2026-11-05 改官方
// 算术平均**——原"属性对相乘"口径作废，场景数值同步；官方三例互证见百科 03 章 §2.4）：
//   次元→飞行 = 2.0 / 龙→飞行 = 1.0 → 均值 **1.5**（原相乘 2.0）
//   次元→邪灵 = 2.0 / 龙→邪灵 = 2.0 → 均值 **2.0**（原相乘 4.0）
//   次元→暗影 = 0.0            → 单组件载体（见 D 组注）**0.0**（免疫）
// 防御方系别取 `ws.view_elementalAttributes[target]`（与伤害公式同一读取源）。
//
// ⚠️ 实现注记：属性伤害**只跑伤害管线的子集**（`CAP` + `BLOCK` 两个阶段），
//    常规挡伤由 `DamageSnapshot::skip_routine_block` 在 core 默认回调里自跳；
//    系别条件性抵挡（岚"免疫非本系技能伤害"）将来作为插件 BLOCK 条目照常响应。

#include <entities/pet_factory.h>
#include <effects/damage_pipeline.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kZhuangJi = 10001;

// 系别 id（`skill_types`）
constexpr int kFlying = 4;    // 飞行：次元→2.0 龙→1.0 → 均值 1.5（J322）
constexpr int kDemon = 19;    // 邪灵：次元→2.0 龙→2.0 → 均值 2.0（J322）
constexpr int kDark = 13;     // 暗影：次元→0.0 免疫（D 组单组件载体锁 0 语义）

const int kDimDragon[2] = {17, 15};   // 次元·龙（组件对，顺序与 skill_types.en 一致）
const int kDimOnly[2] = {17, 0};      // 次元单系（D 组免疫载体，J322 见断言注）

constexpr int kPoints = 100;   // 声明点数

std::array<int, 5> plain_skills() {
    return {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi};
}
ElfPet plain_pet(int hp = 100000) {
    return make_pet(1, "靶子", plain_skills(), /*speed=*/100, /*atk=*/100, hp);
}

// 红伤**攻击**管线探针（GUARD_DETECT = 管线首阶段）：跑过 = 走了攻击伤害那条路。
int& pipeline_hits() {
    static int n = 0;
    return n;
}
// 落在守方身上的伤害事件（EVENT_TAKE_DAMAGE）
int& red_event_total() {
    static int v = 0;
    return v;
}
int& red_event_count() {
    static int n = 0;
    return n;
}

struct Opts {
    bool pink_immune = false;   // 免粉伤票
    bool red_immune = false;    // **常规挡伤**票（ImmunityType::DAMAGE）
    int  shield = 0;            // 护盾点数（红伤才响应）
    bool use_true_damage = false;  // 换成真实伤害做对照
    int  cap_to = -1;           // >0 = 注册 CAP 锁伤（把本次锁到该值）
    int  add_pct = 0;           // 攻击方增伤%（ws.damage_add_pct）
    int  reduce_flat = 0;       // >0 = 注册 REDUCE_FLAT 点数减伤
};

// 一次"属性伤害"结算：返回守方实际失血。
// pipeline_seen：回填**攻击**管线（damage_pipeline_）的 GUARD_DETECT/AMP 是否跑过。
// attacker_elem：攻方系别载体（缺省 kDimDragon 双组件；D 组传 kDimOnly 单组件锁免疫）。
int deal_and_measure(int defender_elem0, int defender_elem1, int& pipeline_seen,
                     const Opts& o = {}, const int* attacker_elem = nullptr) {
    ScenarioBattle b(make_party(plain_pet(1000)), make_party(plain_pet(100000)));
    b.start();
    BattleContext& ctx = b.ctx();
    pipeline_hits() = 0;
    red_event_total() = 0;
    red_event_count() = 0;
    pipeline_seen = 0;

    ctx.ws.view_elementalAttributes[1][0] = defender_elem0;
    ctx.ws.view_elementalAttributes[1][1] = defender_elem1;

    if (o.pink_immune) {
        ctx.grant_immunity(/*owner=*/1, ImmunityType::PINK_DAMAGE, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/1, /*source_id=*/90079);
    }
    if (o.red_immune) {
        ctx.grant_immunity(/*owner=*/1, ImmunityType::DAMAGE, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/1, /*source_id=*/90080);
    }
    if (o.shield > 0) {
        ctx.getPet(1).shield_bank_.add(/*priority=*/0, o.shield, /*source_id=*/90078,
                                       /*duration=*/0);
    }
    if (o.cap_to > 0) {
        ctx.register_damage_effect(DamagePhase::CAP, /*owner=*/1, DamageEffectCategory::MITIGATE,
                                   [cap = o.cap_to](BattleContext* c, int) {
                                       if (c->resolvedDamage.final > cap) {
                                           c->resolvedDamage.final = cap;
                                       }
                                   });
    }
    if (o.reduce_flat > 0) {
        ctx.register_damage_effect(DamagePhase::REDUCE_FLAT, /*owner=*/1,
                                   DamageEffectCategory::MITIGATE,
                                   [amt = o.reduce_flat](BattleContext* c, int) {
                                       c->resolvedDamage.final =
                                           std::max(0, c->resolvedDamage.final - amt);
                                   });
    }
    if (o.add_pct != 0) {
        ctx.ws.damage_add_pct[0] = o.add_pct;   // 攻击方"造成攻击伤害提升X%"（AMP 阶段读）
    }
    ctx.register_damage_effect(
        DamagePhase::GUARD_DETECT, /*owner=*/1, DamageEffectCategory::DETECT,
        [](BattleContext*, int) { ++pipeline_hits(); });
    ctx.register_damage_effect(
        DamagePhase::AMP, /*owner=*/1, DamageEffectCategory::DETECT,
        [](BattleContext*, int) { ++pipeline_hits(); });
    ctx.event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, 0, ctx.roundCount, /*duration_rounds=*/0, /*once=*/false,
        [](BattleContext*, const BattleEvent& ev) {
            if (ev.target == 1 && ev.amount > 0) {
                red_event_total() += ev.amount;
                ++red_event_count();
            }
        },
        WatcherScope::TEAM);

    const int before = ctx.getPet(1).hp;
    if (o.use_true_damage) {
        deal_true_damage(&ctx, /*target=*/1, kPoints, /*actor=*/0);
    } else {
        deal_attribute_damage(&ctx, /*target=*/1, kPoints,
                              attacker_elem ? attacker_elem : kDimDragon, /*actor=*/0);
    }
    // ⚠️ 事件是"入队 + 晚 drain"的（`emit` 只入队，FSM 在时点桶后 drain）。本函数**不驱动 FSM**
    //    （直接调原语）→ 不手动 drain 的话 watcher 永远收不到投递。nullptr = 跳过作废判定。
    ctx.event_center_.drain(&ctx, ctx.roundCount, nullptr);
    pipeline_seen = pipeline_hits();
    return before - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A–D. 数值 = 声明点数 × 克制倍数（防御方系别取视图）
    // ════════════════════════════════════════════════════════════════
    {
        int seen = 0;
        CHECK_EQ(deal_and_measure(0, 0, seen), kPoints, "A) 1x 基准：扣血 == 声明点数");
        std::cout << "  [A] 1x 基准：扣血 " << kPoints << " 通过\n";
    }
    {
        int seen = 0;
        const int loss = deal_and_measure(kFlying, 0, seen);
        CHECK_EQ(loss, kPoints + kPoints / 2,
                 "B) ★ 克制：扣血 == 点数 × 1.5（次元→飞行 2.0 / 龙→飞行 1.0 均值，J322 口径）");
        std::cout << "  [B] 克制 1.5x（飞行均值）：扣血 " << loss << " 通过\n";
    }
    {
        int seen = 0;
        const int loss = deal_and_measure(kDemon, 0, seen);
        CHECK_EQ(loss, kPoints * 2,
                 "C) ★ 双克均值：扣血 == 点数 × 2（2.0 与 2.0 均值 = 2.0，J322 口径；"
                 "原相乘口径 4.0 已作废）");
        std::cout << "  [C] 双克均值 2x（邪灵）：扣血 " << loss << " 通过\n";
    }
    {
        // J322 注：均值口径下双组件 {17,15} 对暗影 = (0.0 + 1.0)/2 = 0.5，不再免疫；
        // "倍率 0 = 免疫"语义改用**单组件**载体（次元→暗影 0.0）锁定。
        int seen = 0;
        const int loss = deal_and_measure(kDark, 0, seen, {}, kDimOnly);
        CHECK_EQ(loss, 0, "D) ★ 倍率 0（次元→暗影 免疫，单组件载体）：不打伤害");
        CHECK_EQ(red_event_count(), 0, "D) 且不发 EVENT_TAKE_DAMAGE");
        std::cout << "  [D] 免疫 0x（暗影，单组件）：不打伤害 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E. **以红伤形式结算**（算红伤落血、发 EVENT_TAKE_DAMAGE、不吃粉伤减免）
    //    ⚠️ 但**护盾不吸收它**（用户 2026-09-18 口径："属性直伤其实不会被护盾/护罩抵挡"）
    //       —— 这是与普通红伤、也是与"真伤"都不同的第三态：
    //       · 区别于普通红伤：护盾不响应（用 `DamageKind::ATTRIBUTE` 落血）
    //       · 区别于真伤：真伤"无法减免"，属性伤害仍吃锁伤/系别抵挡（F/G 组）
    // ════════════════════════════════════════════════════════════════
    {
        int seen = 0;
        Opts o;
        o.shield = 300;
        CHECK_EQ(deal_and_measure(0, 0, seen, o), kPoints,
                 "E1) ★ 有 300 点护盾 → 属性伤害**照打全额**（护盾不吸收属性伤害）");

        Opts t;
        t.shield = 300;
        t.use_true_damage = true;
        CHECK_EQ(deal_and_measure(0, 0, seen, t), kPoints,
                 "E2) 对照：真伤同样不被护盾吸收（两者都穿盾，但原因不同）");

        Opts p;
        p.pink_immune = true;
        CHECK_EQ(deal_and_measure(0, 0, seen, p), kPoints, "E3) ★ 免**粉**票 → 照打全额（= 不是粉伤）");

        Opts plain;
        const int loss = deal_and_measure(0, 0, seen, plain);
        CHECK_EQ(red_event_count(), 1, "E4) 落血照常发一次 EVENT_TAKE_DAMAGE（是红伤落血）");
        CHECK_EQ(red_event_total(), loss, "E5) 事件金额 == 实际失血");
        std::cout << "  [E] 红伤落血：有盾→" << kPoints << "（不吸收）/ 免粉→" << kPoints
                  << " / 红伤事件 1 次 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // F. **受锁伤修正影响**（CAP 阶段生效）
    // ════════════════════════════════════════════════════════════════
    {
        int seen = 0;
        Opts o;
        o.cap_to = 50;   // 先算 100×1.5=150（飞行均值，J322），再被锁到 50
        const int loss = deal_and_measure(kFlying, 0, seen, o);
        CHECK_EQ(loss, 50, "F) ★ 锁伤生效：100×1.5=150 被 CAP 锁到 50");
        std::cout << "  [F] 锁伤：150 → " << loss << " 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // G. **常规挡伤不触发**（"免疫下N次攻击伤害"票）
    //    ⚠️ 这是与普通红伤的关键差别：同样的票对攻击伤害会整段归零。
    // ════════════════════════════════════════════════════════════════
    {
        int seen = 0;
        Opts o;
        o.red_immune = true;
        CHECK_EQ(deal_and_measure(0, 0, seen, o), kPoints,
                 "G) ★ 免**红伤**（常规挡伤）票 → 属性伤害**照打全额**（不吃常规挡伤）");
        // 注："同一张票对**攻击伤害**是有效的"由场景 075 的 B2 组锁定（那里是攻击管线），
        //     本组只锁"它对属性伤害无效"这一半。
        std::cout << "  [G] 常规挡伤：免红伤票 → 仍打 " << kPoints << " 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // H. **不吃攻击伤害的增/减伤**（只跑管线子集 CAP+BLOCK）
    // ════════════════════════════════════════════════════════════════
    {
        int seen = 0;
        Opts a;
        a.add_pct = 50;   // "造成攻击伤害提升50%"（AMP 阶段读 ws.damage_add_pct）
        CHECK_EQ(deal_and_measure(0, 0, seen, a), kPoints,
                 "H1) ★ 增伤50% → 属性伤害**不变**（AMP 阶段不跑）");

        Opts r;
        r.reduce_flat = 30;
        CHECK_EQ(deal_and_measure(0, 0, seen, r), kPoints,
                 "H2) ★ 点数减伤30 → 属性伤害**不变**（REDUCE_FLAT 阶段不跑）");

        Opts plain;
        const int loss = deal_and_measure(0, 0, seen, plain);
        CHECK(loss > 0, "H3) 前置：确实造成了伤害");
        CHECK_EQ(seen, 0, "H4) ★ 攻击管线的 GUARD_DETECT/AMP 都没跑（只跑 CAP+BLOCK 子集）");
        std::cout << "  [H] 不吃增/减伤：增50%→" << kPoints << " / 减30→" << kPoints
                  << " / 攻击管线 0 次 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // I. **穿透限伤凭证可无视锁伤** —— 凭证按用户 2026-09-18 的口径**挂在 RuleCenter 的两个门上**：
    //      `PENETRATE_ATTACK`（攻击技能那条路）/ `PENETRATE_ATTRIBUTE`（属性伤害这条）
    //    I1 凭证来自**所选技能自带** 697（无附加词条 → 属性伤害同样吃）
    //    I2 **薇尔诗·白皑之纷争式**「直接无视、没有附加词条」→ **两条门都登记**、
    //       窗口型（rounds=1/counts=0）→ 属性侧生效，且**重复查询都不失效**
    //    I3 **霍光·无罔之心式**「下两次**攻击技能**无视」→ **只登记攻击门**（counts=2/ON_STAGE）
    //       → 属性伤害查的是另一个门：**既不命中、也不消耗**
    // ════════════════════════════════════════════════════════════════
    {
        // 打两笔属性伤害（对照 I2 的"不失效"与 I3 的"不消耗"）
        // gate：-1 = **不授予任何凭证**（I0 基线）；否则在指定门上授予窗口型凭证
        auto measure_twice = [](bool skill_own_697, int gate,
                                bool again_only /*第二笔*/) {
            ScenarioBattle b(make_party(plain_pet(1000)), make_party(plain_pet(100000)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.ws.view_elementalAttributes[1][0] = kFlying;   // 1.5x → 150（J322 均值）
            ctx.ws.view_elementalAttributes[1][1] = 0;
            if (skill_own_697) {
                ctx.getPet(0).skills[0].penetration_flags.ignore_damage_limit = true;
            }
            // 本回合"所选技能"（凭证分支 ① 要读它）
            ctx.roundChoice[0][0] = 1;   // = SELECT_SKILL
            ctx.roundChoice[0][1] = 0;
            // CAP 锁伤：150 → 50
            ctx.register_damage_effect(DamagePhase::CAP, /*owner=*/1,
                                       DamageEffectCategory::MITIGATE,
                                       [](BattleContext* c, int) {
                                           if (c->resolvedDamage.final > 50) {
                                               c->resolvedDamage.final = 50;
                                           }
                                       });
            if (gate >= 0) {
                // 窗口型（rounds=1/counts=0）→ 当回合有效、查询多次不失效
                ctx.grant_penetration(0, /*level=*/1, /*i_ai=*/false, /*i_dl=*/true,
                                      /*counts=*/0, /*rounds=*/1,
                                      static_cast<RuleCategory>(gate));
            }
            const int before1 = ctx.getPet(1).hp;
            deal_attribute_damage(&ctx, /*target=*/1, kPoints, kDimDragon, /*actor=*/0);
            const int first = before1 - ctx.getPet(1).hp;
            if (!again_only) {
                return first;
            }
            const int before2 = ctx.getPet(1).hp;
            deal_attribute_damage(&ctx, /*target=*/1, kPoints, kDimDragon, /*actor=*/0);
            return before2 - ctx.getPet(1).hp;   // 第二笔
        };
        constexpr int kNoGrant = -1;
        const int kAttrGate = static_cast<int>(RuleCategory::PENETRATE_ATTRIBUTE);
        const int kAtkGate = static_cast<int>(RuleCategory::PENETRATE_ATTACK);

        // I0 无凭证 → 锁伤生效
        CHECK_EQ(measure_twice(/*skill_own_697=*/false, kNoGrant, /*again_only=*/false), 50,
                 "I0) 无凭证 → 锁伤生效（150 → 50）");
        // I1 技能自带 697 → 无视锁伤
        CHECK_EQ(measure_twice(/*skill_own_697=*/true, kNoGrant, /*again_only=*/false), 150,
                 "I1) ★ 所选技能自带 697 → **无视锁伤**（150 全额，J322 均值口径）");
        // I2 白皑之纷争式：属性门登记窗口型凭证 → 生效
        CHECK_EQ(measure_twice(/*skill_own_697=*/false, kAttrGate, /*again_only=*/false), 150,
                 "I2) ★ 属性门登记了凭证（白皑之纷争式）→ 无视锁伤");
        CHECK_EQ(measure_twice(/*skill_own_697=*/false, kAttrGate, /*again_only=*/true), 150,
                 "I3) ★ **窗口型**：同一回合再打一笔**仍然无视**（查询多次不失效）");
        // I4 无罔之心式：只登记攻击门 → 属性这条路上不命中
        CHECK_EQ(measure_twice(/*skill_own_697=*/false, kAtkGate, /*again_only=*/false), 50,
                 "I4) ★ 只登记**攻击门**（无罔之心「下两次攻击技能无视」）→ 属性伤害**不命中**，锁伤照生效");

        // I5 攻击门上的**次数型**凭证：攻击路径每用一次技能消费一次（此处直接验 RuleCenter 语义）
        ScenarioBattle b2(make_party(plain_pet(1000)), make_party(plain_pet(1000)));
        b2.start();
        BattleContext& ctx2 = b2.ctx();
        ctx2.grant_penetration(0, /*level=*/1, /*i_ai=*/false, /*i_dl=*/true, /*counts=*/2,
                               /*rounds=*/0, RuleCategory::PENETRATE_ATTACK);
        CHECK_EQ(ctx2.rule_center_.count_penetrate(0, RuleCategory::PENETRATE_ATTACK), 1,
                 "I5) 攻击门：授予 2 次 → 1 条条目");
        CHECK(ctx2.rule_center_.query_penetrate(0, RuleCategory::PENETRATE_ATTACK,
                                                ctx2.roundCount).valid,
              "I5) 查询命中");
        ctx2.consume_penetration_grants_after_attack(0);   // 攻击一次 → 扣一次
        CHECK_EQ(ctx2.rule_center_.count_penetrate(0, RuleCategory::PENETRATE_ATTACK), 1,
                 "I5) 2→1：次数型扣到 0 才注销（条目仍在）");
        ctx2.consume_penetration_grants_after_attack(0);   // 再攻击一次 → 用尽
        CHECK_EQ(ctx2.rule_center_.count_penetrate(0, RuleCategory::PENETRATE_ATTACK), 0,
                 "I5) 2 次用尽 → 条目注销");
        CHECK_EQ(ctx2.rule_center_.count_penetrate(0, RuleCategory::PENETRATE_ATTRIBUTE), 0,
                 "I5) 属性门始终没有条目（攻击门与该路无关）");
        std::cout << "  [I] 穿透限伤：无凭证→50 / 技能自带697→150 / 属性门窗口凭证→150(重复仍150)"
                  << " / 仅攻击门→50 / 次数型 2 次用尽注销 通过\n";
    }

    std::cout << "scenario_078 PASS（属性伤害：点数 × 克制倍数；吃锁伤、不吃常规挡伤与增/减伤）\n";
    return 0;
}
