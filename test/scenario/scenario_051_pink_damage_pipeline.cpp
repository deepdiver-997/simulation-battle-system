// 场景 051：粉伤通道（PinkDamagePipeline）+ 多段粉 + 两个真实魂印子句。
//
// 机制口径（《赛学必修8—免疫粉伤》流程图 + L445/L463/L338，用户 2026-09-15 确认）：
//   · 粉伤**不走红伤管线**：没有攻击方/技能/克制/暴击那一套输入，也不吃红伤的点数减伤/
//     百分比减伤/锁伤。它有一条**更短的独立管线**：
//       抗性免减 → 免疫粉伤 → 百分比免减（乘法）→ **粉伤提升 n%** → 上限/锁伤 → **护罩** → 检测
//     ★ 增粉在**所有免减之后**、护罩之前（图序如此）——不是链首，P8 专门锁这一条。
//   · **多段粉 = 每次 deal_pink_damage 调用 = 一段**，逐段独立结算（各自取整/各自扣护罩/
//     各自检测）。官方算例 `300×0.65` 逐段取整后相加，而不是 `(300×4)×0.65`——合并会算错。
//     引擎端没有"段计数"这种东西，"调 N 次"就是"N 段"。
//   · 免粉补偿一类**只看体力有没有降**（抗性 100% 免粉 与 效果减粉不区分）：
//     `EVENT_TAKE_PINK_DAMAGE` 只在**经过护罩抵消后仍有剩余结算到本体**时才发。
//
// 验证点：
//   P1 抗性取整逐段发生：6 段各 50、固定抗性 33% → 每段 34、共 204（合并成 300 只会是 201）
//   P2 特效免减排在抗性**之后**（乘法连乘），与"抗性+免减一次算完"结果不同
//   P3 ★ 护罩最后：先算完免减再扣护罩；全被吃掉 → 体力不变、**不发事件**
//   P4 ★ 事件只在"结算到本体"时发：吃掉一段没事件，部分吃掉按**落到本体的量**发
//   P5 ★ 沧岚 2343「此护盾存在时自身受到的固定伤害、百分比伤害不超过此护盾的数值」= CAP 阶段
//   P6 ★★ 箫澈 2099「每次自身受到≥300的固定伤害、百分比伤害」= **逐段**判定 —
//        3 段各 200（合计 600）**不触发**；1 段 600 **触发**
//   P7 ★ 同一魂印的"被护罩抵消"推论：护罩吃光的那一段不触发悲龙之鸣
//   P8 ★ 增粉排在所有免减**之后**（图序）——加法探针把位置做成可观测
//   P9 ★ 沧岚限粉只认**登场盾那一笔**（叠加别的盾也不放宽），且登场盾优先被消耗
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kGaoSu = 20017;   // 高速移动：属性技能（占位，本场景不驱动攻击）
constexpr int kMaxHp = 3000;

constexpr int kCanglanMark = 2343;   // 怒涛·沧岚
constexpr int kXiaocheMark = 2099;   // 箫澈

// p0 = 施加粉伤的一方；p1 = 承受方。
scenario::ScenarioBattle make_battle(SoulMark p1_mark = {},
                                     int fixed_resist = 0,
                                     int p1_hp = kMaxHp) {
    auto p0 = scenario::make_pet(0, "施粉方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/300, /*atk=*/100, /*hp=*/kMaxHp);
    auto p1 = scenario::make_pet(1, "承受方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/100, /*atk=*/100, p1_hp, std::move(p1_mark));
    // ⚠️ 抗性必须在 `start()` **之前**写：ws.eff_*_resist_pct 是回合开始时从 pet 本体同步的
    //    有效视图（sync_damage_resist_view），start 之后再改本体不会反映到视图。
    p1.damage_resist.fixed_pct = fixed_resist;
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

// 施一段粉伤并**当场 drain 事件**（与 battleFsm.cpp:808 的 drain 同款）——
// 魂印 watcher 是在 drain 点才跑的，不 drain 就看不到触发器。
int g_pink_events = 0;
int g_last_pink_amount = 0;

void install_pink_event_probe(BattleContext& ctx, int owner) {
    ctx.event_center_.register_watcher(
        EventType::EVENT_TAKE_PINK_DAMAGE, owner, ctx.roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [](BattleContext*, const BattleEvent& ev) {
            ++g_pink_events;
            g_last_pink_amount = ev.amount;
        },
        WatcherScope::TEAM);
}

void reset_probe() {
    g_pink_events = 0;
    g_last_pink_amount = 0;
}

// 空转一回合（双方都用属性技能）。`start()` 只跑到 OPERATION_ENTER_EXIT_STAGE，
// 回合开始的 ws 同步（含 `sync_damage_resist_view` 把本体抗性刷进有效视图）要等第一个
// ROUND_START 才发生——所以要先空转一回合，粉伤才吃得到抗性。
void idle_round(scenario::ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// 打一段粉伤，返回实际掉血量。
int pink_hit(scenario::ScenarioBattle& b, int amount,
             DamageKind kind = DamageKind::FIXED, int actor = 0) {
    BattleContext& ctx = b.ctx();
    const int before = ctx.getPet(1).hp;
    deal_pink_damage(&ctx, /*target=*/1, amount, kind, actor);
    ctx.event_center_.drain(&ctx, ctx.roundCount);
    return before - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== P1 抗性取整**逐段**发生 ==========
    {
        // 固定抗性 33%：每段 50 → 50 − ⌊50×33/100⌋ = 50 − 16 = 34；6 段共 204。
        // 若把 6 段合并成一段 300 → 300 − ⌊300×33/100⌋ = 300 − 99 = 201 ——**差 3 点**。
        scenario::ScenarioBattle b = make_battle({}, /*fixed_resist=*/33);
        b.start();
        idle_round(b);
        int total = 0;
        for (int i = 0; i < 6; ++i) {
            total += pink_hit(b, 50);
        }
        CHECK_EQ(total, 34 * 6, "P1) 6 段各 50、抗性 33% → 每段 34（逐段取整），共 204");
        CHECK(total != 201, "P1) ★ 与「合并成 300 一段」的 201 不同 —— 逐段结算是可观测的");
        std::cout << "  [P1] 6×50 / 抗性33% → 合计 " << total
                  << "（合并成一段 300 只会是 " << (300 - 300 * 33 / 100) << "）\n";
    }

    //========== P2 特效免减排在抗性之后（乘法连乘）==========
    {
        // 抗性 50% + 减粉 50%：`300 → 150 → 75`（两次各自取整）。
        // 若把两条免减合成 100% 一次算 → 0，差别明显。
        scenario::ScenarioBattle b = make_battle({}, /*fixed_resist=*/50);
        b.start();
        idle_round(b);
        b.ctx().pink_reduce_pct[1] = 50;   // 效果授予的特效免减（REDUCE_EXTRA 阶段）
        const int taken = pink_hit(b, 300);
        CHECK_EQ(taken, 75, "P2) 抗性 50% → 150，再乘特效免减 50% → 75（乘法连乘，非相加）");
        std::cout << "  [P2] 300 / 抗性50% / 免减50% → " << taken << "（乘算链，不是 0）\n";
    }

    //========== P3 ★ 护罩最后：先算完免减再扣护罩 ==========
    {
        // 护罩 100。先减抗性 50%（200 → 100），**再**扣护罩 100 → 全被吃掉、体力不变。
        // 若护罩在前（200 − 100 = 100，再减半 = 50）就会掉 50 —— 顺序可观测。
        scenario::ScenarioBattle b = make_battle({}, /*fixed_resist=*/50);
        b.start();
        idle_round(b);
        b.ctx().getPet(1).hood_bank_.add(/*priority=*/0, /*quantity=*/100, /*source_id=*/7,
                                         /*duration=*/0);
        const int taken = pink_hit(b, 200);
        CHECK_EQ(taken, 0, "P3) ★ 抗性先算（200→100）再扣护罩 100 → 体力不变");
        CHECK_EQ(b.ctx().getPet(1).hood_bank_.total(), 0, "P3) 护罩被吃光");
        std::cout << "  [P3] 护罩 100 + 抗性 50%：200 点粉伤 → 掉 " << taken
                  << "（护罩在免减之后扣）\n";
    }

    //========== P4 ★ 事件只在"结算到本体"时发 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        b.ctx().getPet(1).hood_bank_.add(/*priority=*/0, /*quantity=*/100, /*source_id=*/7,
                                         /*duration=*/0);
        install_pink_event_probe(b.ctx(), /*owner=*/1);

        reset_probe();
        const int d1 = pink_hit(b, 80);   // 全被护罩吃掉
        CHECK_EQ(d1, 0, "P4) 80 点全被护罩吃掉 → 体力不变");
        CHECK_EQ(g_pink_events, 0, "P4) ★ 没结算到本体 → **不发**粉伤事件");

        reset_probe();
        const int d2 = pink_hit(b, 150);  // 护罩吃 20，剩 130 落到本体
        CHECK_EQ(d2, 130, "P4) ★ 护罩只剩 20 → 130 落到本体");
        CHECK_EQ(g_pink_events, 1, "P4) ★ 有剩余结算到本体 → 发一次事件");
        CHECK_EQ(g_last_pink_amount, 130, "P4) ★ 事件里的量是**实际扣血量**（130），不是原始量 150");
        std::cout << "  [P4] 护罩 100：80 点 → 掉 0 / 事件 0 次；150 点 → 掉 130 / 事件 1 次"
                  << "（amount=" << g_last_pink_amount << "）\n";
    }

    //========== P5 ★ 沧岚 2343：粉伤不超过护盾值（CAP 阶段）==========
    {
        // 登场给 400 护盾；此后固定/百分比伤害都不超过 400。
        scenario::ScenarioBattle b = make_battle(SoulMark(kCanglanMark, "怒涛·沧岚", "沧岚"));
        b.start();
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(1).shield_bank_.total(), 400, "P5) 登场获得 400 点护盾");
        const int big = pink_hit(b, 600);
        CHECK_EQ(big, 400, "P5) ★ 600 点固定伤害被截断到护盾值 400");
        const int pct = pink_hit(b, 600, DamageKind::PERCENT_VALUE);
        CHECK_EQ(pct, 400, "P5) ★ 百分比伤害同样被截断（条款覆盖固定 + 百分比两档）");
        CHECK_EQ(b.ctx().getPet(1).shield_bank_.total(), 400,
                 "P5) 上限条款只是**截断**，不消耗护盾值本身");
        std::cout << "  [P5] 沧岚 2343：护盾 400 → 600 点固定伤害只掉 " << big
                  << "、600 点百分比伤害只掉 " << pct << "（护盾未被消耗）\n";

        // 对照组：没有魂印 → 不截断
        scenario::ScenarioBattle c = make_battle();
        c.start();
        CHECK_EQ(pink_hit(c, 600), 600, "P5) 对照：没有 2343 → 600 点照掉（确认是魂印在起作用）");
    }

    //========== P6 ★★ 箫澈 2099：逐段判定 ≥300 ==========
    {
        // 3 段各 200（合计 600）**不触发**；1 段 600 **触发**。
        scenario::ScenarioBattle b = make_battle(SoulMark(kXiaocheMark, "箫澈", "箫澈"));
        b.start();
        idle_round(b);
        install_pink_event_probe(b.ctx(), /*owner=*/1);

        reset_probe();
        int total = 0;
        for (int i = 0; i < 3; ++i) {
            total += pink_hit(b, 200);
        }
        CHECK_EQ(total, 600, "P6) 3 段各 200 全部落到本体（合计 600）");
        CHECK_EQ(b.ctx().getPet(1).hp, kMaxHp - 600,
                 "P6) ★★ 合计 600 > 300 但**没有触发**悲龙之鸣 —— 判定是**逐段**的");
        CHECK(!b.ctx().is_immune(1, ImmunityType::DAMAGE, State::BATTLE_FIRST_ATTACK_DAMAGE),
              "P6) ★ 未触发 → 没有获得「免疫下1次攻击伤害」");
        std::cout << "  [P6] 3 段各 200（合计 600）→ 体力 " << b.ctx().getPet(1).hp
                  << "，不触发（逐段判定）\n";

        // 单段 600 → 本段 ≥300 → 触发：免疫下1次攻击伤害 + 恢复最大体力 1/3
        const int before = b.ctx().getPet(1).hp;
        pink_hit(b, 600);
        const int after = b.ctx().getPet(1).hp;
        // ⚠️ 不能用 pink_hit 的返回值（它是"扣血差"）：触发悲龙之鸣会**回血**，
        //    差值会被回血抵消成负数。直接比最终体力。
        CHECK_EQ(after, before - 600 + kMaxHp / 3,
                 "P6) ★★ 触发悲龙之鸣 → 恢复最大体力 1/3（+1000）");
        CHECK(b.ctx().is_immune(1, ImmunityType::DAMAGE, State::BATTLE_FIRST_ATTACK_DAMAGE),
              "P6) ★ 触发 → 获得「免疫下1次受到的攻击伤害」（次数型，由红伤管线 BLOCK 阶段消费）");
        std::cout << "  [P6] 单段 600 → 掉 600 后回 " << (kMaxHp / 3) << " → 体力 " << after
                  << "，并挂上 1 次攻击伤害免疫\n";
    }

    //========== P7 ★ 护罩吃光的那一段不触发悲龙之鸣 ==========
    {
        // 用户口径的直接推论：这类检测就是"即时算一段粉、当场应用、再看体力降没降"。
        // 护罩吃光 → 体力没变 → 不发事件 → 不触发（哪怕原始量 600 ≥ 300）。
        scenario::ScenarioBattle b = make_battle(SoulMark(kXiaocheMark, "箫澈", "箫澈"));
        b.start();
        idle_round(b);
        b.ctx().getPet(1).hood_bank_.add(/*priority=*/0, /*quantity=*/1000, /*source_id=*/7,
                                         /*duration=*/0);
        const int d = pink_hit(b, 600);
        CHECK_EQ(d, 0, "P7) 600 点全被护罩吃掉 → 体力不变");
        CHECK(!b.ctx().is_immune(1, ImmunityType::DAMAGE, State::BATTLE_FIRST_ATTACK_DAMAGE),
              "P7) ★ 被护罩抵消 → 没有「受到粉伤」 → 不触发悲龙之鸣");
        std::cout << "  [P7] 护罩 1000 吃下 600 点粉伤 → 体力不变、不触发悲龙之鸣\n";
    }

    //========== P8 ★ 增粉排在所有免减**之后**（《赛学必修8》流程图）==========
    {
        // 图序：抗性 → 免疫 → 百分比免减 → **粉伤提升 n%** → 护罩。
        // 百分比增粉与抗性近似可交换（只差取整），所以用**加法**探针把位置做成可观测：
        //   正序 200 →抗性50%→ 100 →+50→ 150 ／ 错序 200 →+50→ 250 →抗性50%→ 125。
        scenario::ScenarioBattle b = make_battle({}, /*fixed_resist=*/50);
        b.start();
        idle_round(b);
        b.ctx().register_pink_damage_effect(
            PinkDamagePhase::AMP, /*owner=*/1,
            [](BattleContext* c, int) {
                if (c->resolvedPink.final > 0) {
                    c->resolvedPink.final += 50;   // 探针：把"增粉"这一格的内容置为 +50 点
                }
            });
        const int taken = pink_hit(b, 200);
        CHECK_EQ(taken, 150, "P8) ★ 增粉在免减**之后**：(200 →抗性50%→ 100) +50 = 150");
        CHECK(taken != 125, "P8) ★ 不是「先增粉再免减」的 125 —— 顺序可观测");
        std::cout << "  [P8] 抗性50% + 增粉50点：200 点粉伤 → 掉 " << taken
                  << "（先免减后增粉；反序会得到 125）\n";
    }

    //========== P9 ★ 沧岚限粉只认**登场盾那一笔**，且它与别的盾叠加时优先被消耗 ==========
    {
        scenario::ScenarioBattle b = make_battle(SoulMark(kCanglanMark, "怒涛·沧岚", "沧岚"));
        b.start();
        idle_round(b);
        ShieldBank& bank = b.ctx().getPet(1).shield_bank_;
        bank.add(/*priority=*/0, /*quantity=*/300, /*source_id=*/7, /*duration=*/0);  // 普通盾
        CHECK_EQ(bank.total(), 700, "P9) 登场盾 400 + 普通盾 300");

        // ① 上限只认登场盾那 400 —— 不是 total() 的 700
        CHECK_EQ(pink_hit(b, 1000), 400, "P9) ★ 限粉只认登场盾：1000 点被截到 400（不是 700）");

        // ② 红伤叠加时**优先消耗登场盾**（priority = kSoulShieldPriority）
        const int hp_before_red = b.ctx().getPet(1).hp;
        deal_damage(&b.ctx(), /*target=*/1, /*amount=*/600, DamageKind::NORMAL, /*actor=*/0);
        CHECK_EQ(bank.quantity_of(kCanglanMark), 0, "P9) ★ 600 红伤先吃光登场盾（高优先级）");
        CHECK_EQ(bank.quantity_of(7), 100, "P9) 剩下 200 才轮到普通盾 → 普通盾余 100");
        CHECK_EQ(b.ctx().getPet(1).hp, hp_before_red, "P9) 两笔盾合起来 700 > 600 → 体力未动");

        // ③ 登场盾没了 → 「此护盾存在时」不成立 → 限粉失效
        CHECK_EQ(pink_hit(b, 500), 500, "P9) ★ 登场盾打光后限粉失效（500 点照掉）");
        std::cout << "  [P9] 沧岚限粉：只认登场盾 400（叠普通盾也不放宽）；"
                     "600 红伤先吃登场盾→普通盾余 100；登场盾没了限粉即失效\n";
    }

    //========== P10 ★ 次数型免粉只挡**一段**（免粉与挡伤同一条路）==========
    {
        // 免粉 = `ImmunityType::PINK_DAMAGE` 的 RuleCenter 票据（与红伤"挡伤"同一个
        // `grant_immunity` / `is_immune` / `consume_immune`），因此天然带 counts 语义。
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        b.ctx().grant_immunity(1, ImmunityType::PINK_DAMAGE, coverage_all(),
                               /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/0,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/1, /*source_effect_id=*/999);
        const int a1 = pink_hit(b, 100);
        const int a2 = pink_hit(b, 100);
        CHECK_EQ(a1, 0, "P10) 第 1 段被次数型免粉挡下");
        CHECK_EQ(a2, 100, "P10) ★ 次数用完 → 第 2 段照常落到本体（**多段粉只挡一段**）");
        std::cout << "  [P10] 次数型免粉 counts=1：100 点 → 掉 " << a1 << "；再一段 → 掉 " << a2
                  << "（只挡一段）\n";

        // 对照组：窗口型/永久型（counts=0）每段都查、每段都免
        scenario::ScenarioBattle c = make_battle();
        c.start();
        idle_round(c);
        c.ctx().grant_immunity(1, ImmunityType::PINK_DAMAGE, coverage_all());
        const int b1 = pink_hit(c, 100);
        const int b2 = pink_hit(c, 100);
        CHECK_EQ(b1, 0, "P10) 永久免粉：第 1 段免");
        CHECK_EQ(b2, 0, "P10) 永久免粉：第 2 段也免（counts=0 不消费）");
        std::cout << "  [P10] 永久免粉（counts=0）：两段都掉 " << b1 << "/" << b2
                  << "（每段都免）\n";
    }

    //========== P11 ★ 粉转真（免疫粉伤）检测点在**护罩之前** ==========
    {
        // 图里这条分支挂在"粉伤提升 n%"之后、护罩免减之前 —— 于是转成真伤后
        // **护罩根本不会被消费**（真伤直通护盾/护罩）。顺序反了就会白扣护罩。
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        b.ctx().getPet(1).hood_bank_.add(/*priority=*/0, /*quantity=*/500, /*source_id=*/7,
                                         /*duration=*/0);
        b.ctx().grant_immunity(1, ImmunityType::PINK_DAMAGE, coverage_all());
        const int d = pink_hit(b, 100);
        // ★ **没有全局粉转真开关**（用户 2026-09-15 口径）：粉伤即时结算，
        //    效果自己读 `ctx.resolvedPink` —— `final <= 0 && absorbed == 0` = 被免粉挡下。
        const PinkDamageResolved& r = b.ctx().resolvedPink;
        CHECK(r.final <= 0 && r.absorbed == 0, "P11) 本段被免粉挡下（final<=0 且护罩未参与）");
        deal_damage(&b.ctx(), 1, r.raw, DamageKind::TRUE_DMG, 0);   // 效果自己补等量真伤
        CHECK_EQ(d, 0, "P11) 粉伤本身被免 → 掉 0");
        CHECK_EQ(b.ctx().getPet(1).hp, kMaxHp - 100, "P11) 补的等量真伤落到本体（-100）");
        CHECK_EQ(b.ctx().getPet(1).hood_bank_.total(), 500,
                 "P11) ★ 检测点在护罩**之前** → 护罩一点没被消耗（真伤直通）");
        std::cout << "  [P11] 粉转真·免疫粉伤：100 点 → 真伤掉 " << d
                  << "，护罩仍余 " << b.ctx().getPet(1).hood_bank_.total() << "（未被消耗）\n";
    }

    std::cout << "\n场景 051 通过：粉伤走**独立管线**（抗性→免疫→百分比免减→增粉→上限→护罩→检测）；\n"
                 "  多段粉 = 每次调用一段、逐段取整/逐段扣护罩/逐段判定；\n"
                 "  粉伤事件只在**经过护罩抵消后仍有剩余结算到本体**时才发 —— 免粉补偿一类\n"
                 "  因此只需看\"体力有没有降\"，不必区分免疫 / 抗性 100% / 效果减粉。\n";
    return 0;
}
