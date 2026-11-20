// 场景 7790 —— 粉伤管线两笔 S18 清缴实证（08 章待验证清单 #1/#2）：
//
//   A 组 ★ CAP 阶段位于护罩**之前**（08 章 §八#1：battleContext.cpp 自注
//     「位置（护罩之前）待实测确认」——实测确认它）。
//     判别设计：2343 登场盾 400（CAP=quantity_of(2343)）+ 护罩 500 + 粉伤 600。
//       · 正序（CAP→HOOD，kOrder 现序）：600 →CAP 钳 400→ 护罩吃 400 → final=0
//         → 体力不变、护罩余 100、absorbed=400；
//       · 若 CAP 在 HOOD 之后（错误位次）：护罩先吃 500 → final=100 → CAP 钳不动
//         （100≤400）→ 掉 100、护罩 0。
//       两组数字互斥，位次可观测。（无魂印对照组 = 错误位次的数字。）
//
//   B 组 ★ ws.ignore_shield 的粉伤侧覆盖面 = **只跳 HOOD**（08 章 §八#2「星皇之怒
//     粉伤侧凭证」的引擎半边）：官方区间横跨 免疫→百分比免减→增粉→护罩、不含抗性
//     （pink_damage_pipeline.h 流程图注 + battleContext.cpp:296-297 同口径）。
//     引擎现状 ignore_shield 只被 HOOD 默认回调消费——逐相钉死：
//       B1 阳性对照：HOOD 被跳过（护罩不被消耗、全额落血）；
//       B2 IMMUNE 不跳：免粉票在 ignore_shield 下照挡照消费（并验证
//          EVENT_DAMAGE_IMMUNED 照发——09 章 §六.4 的 emit 面实证）；
//       B3 REDUCE_EXTRA 不跳：特效免减照乘；
//       B4 RESIST 不跳：抗性照乘（与官方「不含抗性」一致）；
//       B5 AMP 不跳：增粉探针照加。
//
// 断言纪律：全直调 deal_pink_damage（场景 051 先例），无对局随机、无概率；
//   全部 submit_operation + drive() 有限推进，绝不 while(!FINISHED)。
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kGaoSu = 20017;   // 高速移动：属性技能（占位，不驱动攻击）
constexpr int kMaxHp = 3000;
constexpr int kCanglanMark = 2343;   // 怒涛·沧岚（CAP 首个真实生产者）

// p0 = 施粉方（actor）；p1 = 承受方（target）。
scenario::ScenarioBattle make_battle(SoulMark p1_mark = {},
                                     int fixed_resist = 0) {
    auto p0 = scenario::make_pet(0, "施粉方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/300, /*atk=*/100, /*hp=*/kMaxHp);
    auto p1 = scenario::make_pet(1, "承受方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/100, /*atk=*/100, kMaxHp, std::move(p1_mark));
    // 抗性在 start() 前写本体（sync_damage_resist_view 回合开始刷进有效视图——051 同款）。
    p1.damage_resist.fixed_pct = fixed_resist;
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

int g_pink_events = 0;
int g_immuned_events = 0;

void install_event_probes(BattleContext& ctx, int owner) {
    ctx.event_center_.register_watcher(
        EventType::EVENT_TAKE_PINK_DAMAGE, owner, ctx.roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [](BattleContext*, const BattleEvent&) { ++g_pink_events; },
        WatcherScope::TEAM);
    ctx.event_center_.register_watcher(
        EventType::EVENT_DAMAGE_IMMUNED, owner, ctx.roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [](BattleContext*, const BattleEvent&) { ++g_immuned_events; },
        WatcherScope::TEAM);
}

void reset_probes() {
    g_pink_events = 0;
    g_immuned_events = 0;
}

// 空转一回合：回合开始的 ws 同步（含抗性视图）要等第一个 ROUND_START（051 同款）。
void idle_round(scenario::ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// 施一段粉伤，返回实际掉血量（直调 + 当场 drain，051 同款）。
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

    //========== A1 ★ CAP 在护罩之前：2343 CAP(400) + 护罩 500 + 粉伤 600 ==========
    {
        scenario::ScenarioBattle b =
            make_battle(SoulMark(kCanglanMark, "怒涛·沧岚", "沧岚"));
        b.start();
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(1).shield_bank_.quantity_of(kCanglanMark), 400,
                 "A1) 2343 登场盾 400 在账（CAP 判据 = quantity_of(2343)）");
        b.ctx().getPet(1).hood_bank_.add(/*priority=*/0, /*quantity=*/500, /*source_id=*/7,
                                         /*duration=*/0);
        install_event_probes(b.ctx(), 1);
        reset_probes();
        const int taken = pink_hit(b, 600);
        CHECK_EQ(taken, 0,
                 "A1) ★ CAP 先钳 600→400 → 护罩吃光 400 → 体力不变（错误位次会掉 100）");
        CHECK_EQ(b.ctx().getPet(1).hood_bank_.total(), 100,
                 "A1) ★ 护罩 500 被吃 400 余 100（若护罩先吃 500，余量为 0）");
        const PinkDamageResolved& r = b.ctx().resolvedPink;
        CHECK_EQ(r.absorbed, 400, "A1) absorbed=400 —— 护罩吃的是**钳后**的 400（位次铁证）");
        CHECK_EQ(r.final, 0, "A1) final=0 → 早退不扣血、不发粉伤事件");
        CHECK_EQ(g_pink_events, 0, "A1) 没结算到本体 → 不发 EVENT_TAKE_PINK_DAMAGE");
        std::cout << "  [A1] CAP400+罩500+粉600 → 掉 " << taken
                  << "、罩余 " << b.ctx().getPet(1).hood_bank_.total()
                  << "、absorbed=" << r.absorbed << "（CAP 在护罩之前）\n";
    }

    //========== A2 对照：无 2343（=「CAP 在护罩之后」会出现的数字） ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        b.ctx().getPet(1).hood_bank_.add(/*priority=*/0, /*quantity=*/500, /*source_id=*/7,
                                         /*duration=*/0);
        install_event_probes(b.ctx(), 1);
        reset_probes();
        const int taken = pink_hit(b, 600);
        CHECK_EQ(taken, 100, "A2) 对照：无 CAP → 护罩吃 500、剩 100 落本体");
        CHECK_EQ(b.ctx().getPet(1).hood_bank_.total(), 0, "A2) 对照：护罩吃光");
        CHECK_EQ(g_pink_events, 1, "A2) 对照：有剩余落本体 → 发事件");
        std::cout << "  [A2] 无魂印对照：掉 " << taken << "、罩余 "
                  << b.ctx().getPet(1).hood_bank_.total()
                  << "（=错误位次的数字，与 A1 互斥）\n";
    }

    //========== B1 ★ 阳性对照：ignore_shield 跳过 HOOD ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        b.ctx().getPet(1).hood_bank_.add(/*priority=*/0, /*quantity=*/500, /*source_id=*/7,
                                         /*duration=*/0);
        b.ctx().ws.ignore_shield[0] = true;   // 攻击方持「无视护盾/护罩响应」
        const int taken = pink_hit(b, 300);
        CHECK_EQ(taken, 300, "B1) ★ ignore_shield → HOOD 跳过：300 全额落本体");
        CHECK_EQ(b.ctx().getPet(1).hood_bank_.total(), 500, "B1) ★ 护罩一点没被消耗");
        CHECK_EQ(b.ctx().resolvedPink.absorbed, 0, "B1) absorbed=0（护罩未参与结算）");
        std::cout << "  [B1] ignore_shield：粉 300 → 掉 " << taken << "、罩余 "
                  << b.ctx().getPet(1).hood_bank_.total() << "（HOOD 被跳过）\n";
    }

    //========== B2 ★ IMMUNE 不跳：免粉票照挡照消费 + EVENT_DAMAGE_IMMUNED 照发 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        install_event_probes(b.ctx(), 1);
        b.ctx().grant_immunity(1, ImmunityType::PINK_DAMAGE, coverage_all(),
                               /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/0,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/1, /*source_effect_id=*/999);
        b.ctx().ws.ignore_shield[0] = true;
        reset_probes();
        const int a1 = pink_hit(b, 300);
        CHECK_EQ(a1, 0, "B2) ★ ignore_shield 不覆盖 IMMUNE：免粉票照挡（掉 0）");
        CHECK_EQ(g_immuned_events, 1, "B2) ★ 免疫票命中 → EVENT_DAMAGE_IMMUNED 照发");
        CHECK(!b.ctx().is_immune(1, ImmunityType::PINK_DAMAGE, State::BATTLE_FIRST_ATTACK_DAMAGE),
              "B2) ★ 次数型票被这次「被无视区间外的免粉」消费掉了");
        const int a2 = pink_hit(b, 300);
        CHECK_EQ(a2, 300, "B2) 票已消费 → 第 2 段照常落本体");
        std::cout << "  [B2] ignore_shield + 免粉票：第一段掉 " << a1 << "（票挡下、事件 "
                  << g_immuned_events << " 次）、第二段掉 " << a2 << "（IMMUNE 不在无视区间）\n";
    }

    //========== B3 ★ REDUCE_EXTRA 不跳：特效免减照乘 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        b.ctx().pink_reduce_pct[1] = 50;
        b.ctx().ws.ignore_shield[0] = true;
        const int taken = pink_hit(b, 300);
        CHECK_EQ(taken, 150, "B3) ★ ignore_shield 不覆盖 REDUCE_EXTRA：免减 50% 照乘（150）");
        std::cout << "  [B3] ignore_shield + 特效免减 50%：粉 300 → 掉 " << taken << "\n";
    }

    //========== B4 ★ RESIST 不跳：抗性照乘（与官方「不含抗性」一致） ==========
    {
        scenario::ScenarioBattle b = make_battle({}, /*fixed_resist=*/50);
        b.start();
        idle_round(b);
        b.ctx().ws.ignore_shield[0] = true;
        const int taken = pink_hit(b, 300);
        CHECK_EQ(taken, 150, "B4) ★ ignore_shield 不覆盖 RESIST：抗性 50% 照乘（150）");
        std::cout << "  [B4] ignore_shield + 固定抗性 50%：粉 300 → 掉 " << taken
                  << "（抗性本就在官方无视区间之外）\n";
    }

    //========== B5 ★ AMP 不跳：增粉探针照加 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        b.ctx().register_pink_damage_effect(
            PinkDamagePhase::AMP, /*owner=*/1,
            [](BattleContext* c, int) {
                if (c->resolvedPink.final > 0) {
                    c->resolvedPink.final += 50;
                }
            });
        b.ctx().ws.ignore_shield[0] = true;
        const int taken = pink_hit(b, 200);
        CHECK_EQ(taken, 250, "B5) ★ ignore_shield 不覆盖 AMP：增粉探针 +50 照加（250）");
        std::cout << "  [B5] ignore_shield + 增粉探针 50：粉 200 → 掉 " << taken << "\n";
    }

    std::cout << "\n场景 7790 通过：\n"
                 "  A 组 —— 粉伤 CAP 阶段实测位于护罩之前（600/盾400/罩500：掉 0、罩余 100、\n"
                 "  absorbed=400；错误位次=掉 100、罩尽，A2 对照同数）；\n"
                 "  B 组 —— ws.ignore_shield 粉伤侧覆盖面 = 只跳 HOOD（B1 阳性），IMMUNE/\n"
                 "  REDUCE_EXTRA/RESIST/AMP 四相全部照常（B2-B5）——官方「星皇之怒无视区间\n"
                 "  横跨 免疫→免减→增粉→护罩、不含抗性」在引擎侧的缺口坐实，补齐需粉伤侧\n"
                 "  凭证（内核待办，随星皇线）。\n";
    return 0;
}
