// 场景 7833 —— S20 百科机制穿测②：08 章「粉伤管线 + 双账口径」在**真实战斗流**的
// 端到端（S20，2026-11-05，worktree ../agents/s20；号段 7830-7839 一文件）。
//
// 与既有粉伤场景的分工：051/7790 以直调 deal_pink_damage 逐条锁阶段语义；
// 本场景把它们**串成一场完整对局**（3 回合 INIT→出招→ROUND_END 粉伤落地），
// 粉伤生产者本身走在 FSM 的 BATTLE_ROUND_END 魂印节点里（canglan 2343
// phase_end 同通道），验证的是「一条真实回合流程里十阶段序 + 双账 + 逐段取整
// 同时成立」。全场景 submit×2+drive() 有限推进，无 while(!FINISHED)。
//
// 对应百科正文（docs_local/docs/10-游戏百科/08-护盾护罩粉伤管线.md）：
//   A 组 §三.1 双账分离 + §3.2 优先级消耗 + §2.3 HOOD 阶段 + §2.1 absorbed/溢出：
//     守方带盾 400（红伤账）+ 双笔护罩（300@高优先 + 500@低优先，粉伤账），
//     攻方每回合真实拍打（红伤）+ ROUND_END 两段粉（FIXED 333 + PERCENT_VALUE 300）。
//     A1 R1：红伤吃盾（盾 [298,313]）、粉伤逐段吃罩且**高优先笔先消耗**
//         （§3.2 absorb 从最高优先级开始扣：段1 吃 9003 300→66；段2 吃尽 9003+
//         9002→296，掉 0、absorbed=234/270 全记账、破罩事件 1 次）。
//     A2 R2：再一轮红+粉——罩剩 362：段1 吃 234（9002→62）、段2 破罩溢出
//         208 落本体（§2.1 溢出穿透 + 事件按**实际扣血量**发）。
//     A3 R3：★ 免伤票负面验证（§五 推论 1「免伤/减伤通道彼此独立」）：给守方
//         挂红伤次数票 ImmunityType::DAMAGE counts=2——红伤被票挡下（消费 1 张、
//         场上仍有票），**粉伤照常掉血** 234+270——粉伤不进主伤害管线的负面证。
//   B 组 §六 案例一（2343 护盾门槛，quantity_of 口径）：真实 INIT 的沧岚 2343
//     登场盾 400（early on_enter，非手工 add）→ 攻方真实拍打消耗后，ROUND_END
//     粉伤 600 每回合**实落恰好 = quantity_of(2343) 现值**（实时求值、CAP 只钳
//     不耗）：三轮落体事件 amount = 301 / 301 / 204 —— R1=400-红、R2 轮空不变、
//     R3 再吃一轮红后变小，两个不同现值都精确命中 = 「不缓存 400」的直接证明。
//     实测发现（如实记档，非本单修复项）：沧岚满血局里 2343 的 heal 分支每回合
//     都抵平体力（净掉血 0）——盾吸收的红伤不发 EVENT_TAKE_DAMAGE、ROUND_END
//     桶内粉伤发出的事件在同一桶 pass 内不被 drain，took_attack_damage 旗每轮
//     phase_end 判定时仍未置位 → 走 heal+反弹粉而非发红伤票。故 B 组落体观察
//     走 EVENT_TAKE_PINK_DAMAGE 的 amount（实际扣血量，08 章 §2.5），不吃 heal
//     干扰；这是「2343 受击旗在盾吸红/同桶粉伤下从未置位」的行为级实录。
//   §2.6 多段取整：FIXED 333 × 固定抗性 30% → 333−⌊333×30/100⌋=234；
//     PERCENT_VALUE 300 × 百分比抗性 10% → 270——两段**各走各的抗性通道**
//     （§2.2 tier 分型）、各取整一次；FINAL 探针逐段记账（final, absorbed, tier）。
//
// 断言纪律：精确值 + 理论带（红伤 [87,102] 锚，百科 03 章 §4.3）+ 探针序列逐项
//   相等；无概率分支依赖（抗性/取整全确定）。
#include <iostream>
#include <vector>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;    // 拍打：普通系 物 40（atk300/def100 锚 → raw ∈ [87,102]）
constexpr int kGaoSu = 20017;    // 高速移动：属性技能（占位/轮空）
constexpr int kBigHp = 20000;    // A 战守方体力（整场掉血 712 精确锚）
constexpr int kShieldSrc = 9001;
constexpr int kHoodLowSrc = 9002;    // 低优先护罩笔（500 @prio 10）
constexpr int kHoodHighSrc = 9003;   // 高优先护罩笔（300 @prio 50）

constexpr int kCanglanMark = 2343;   // 怒涛·沧岚（CAP 首个真实生产者）

// ── 粉伤生产者（真实战斗通道：BATTLE_ROUND_END 魂印节点，canglan phase_end 同款）──
// A 战载体：两段粉——FIXED 333 + PERCENT_VALUE 300（混合打击逐段取整）。
EffectResult pink_pair_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int target = 1 - owner;
    deal_pink_damage(ctx, target, 333, DamageKind::FIXED, owner);
    deal_pink_damage(ctx, target, 300, DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

// B 战载体：单段 FIXED 600（撞 2343 CAP 门槛用）。
EffectResult pink_600_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    deal_pink_damage(ctx, 1 - owner, 600, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

SoulMark make_pink_pair_mark() {
    SoulMark sm;
    sm.id = 9994;
    sm.name = "粉伤源·两段(测试)";
    sm.description = "ROUND_END 对对手发 FIXED 333 + PERCENT_VALUE 300（逐段独立结算）";
    sm.args = EffectArgs(std::vector<int>{0, 1});
    sm.program_ = {SoulMarkNodeRef(State::BATTLE_ROUND_END, &pink_pair_effect)};
    sm.has_program_ = true;
    return sm;
}

SoulMark make_pink_600_mark() {
    SoulMark sm;
    sm.id = 9995;
    sm.name = "粉伤源·600(测试)";
    sm.description = "ROUND_END 对对手发 FIXED 600";
    sm.args = EffectArgs(std::vector<int>{0, 1});
    sm.program_ = {SoulMarkNodeRef(State::BATTLE_ROUND_END, &pink_600_effect)};
    sm.has_program_ = true;
    return sm;
}

// ── A 战构局 ──────────────────────────────────────────────────────────────
ElfPet a_attacker() {
    ElfPet p = make_pet(1, "粉伤攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kBigHp, make_pink_pair_mark());
    return p;
}

ElfPet a_defender() {
    ElfPet p = make_pet(2, "双账守方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kBigHp);
    // §2.2 tier 分型的两根钉：FIXED 走固定抗性 30%、PERCENT_VALUE 走百分比抗性 10%
    //（本体槽，回合开始 sync_damage_resist_view 刷进有效视图——051 同款时序）。
    p.damage_resist.fixed_pct = 30;
    p.damage_resist.percent_pct = 10;
    return p;
}

// ── 事件/逐段探针（A 战）────────────────────────────────────────────────
int g_pink_events = 0;
int g_last_pink_amount = -1;
int g_broken_events = 0;
std::vector<std::array<int, 3>> g_seg_log;   // {final, absorbed, tier(0=FIXED,1=PERCENT)}

void install_probes(BattleContext& ctx) {
    ctx.event_center_.register_watcher(
        EventType::EVENT_TAKE_PINK_DAMAGE, /*owner=*/1, ctx.roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [](BattleContext*, const BattleEvent& ev) {
            if (ev.target != 1) {
                return;   // 只记守方侧（2343 heal 分支等他方粉伤不记）
            }
            ++g_pink_events;
            g_last_pink_amount = ev.amount;
        },
        WatcherScope::TEAM);
    ctx.event_center_.register_watcher(
        EventType::EVENT_SHIELD_BROKEN, /*owner=*/1, ctx.roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [](BattleContext*, const BattleEvent& ev) {
            if (ev.target != 1) {
                return;
            }
            ++g_broken_events;
        },
        WatcherScope::TEAM);
    // FINAL 探针：十阶段终点读点，逐段记账（07790 B5 探针同款注册通道）。
    ctx.register_pink_damage_effect(
        PinkDamagePhase::FINAL, /*owner=*/1,
        [](BattleContext* c, int bucket_owner) {
            const PinkDamageResolved& r = c->resolvedPink;
            if (bucket_owner != r.target || r.target != 1) {
                return;
            }
            g_seg_log.push_back({r.final, r.absorbed,
                                 r.tier == PinkDamageTier::PERCENT ? 1 : 0});
        });
}

int g_b_pink_events = 0;
int g_b_last_amount = -1;

void install_b_probe(BattleContext& ctx) {
    ctx.event_center_.register_watcher(
        EventType::EVENT_TAKE_PINK_DAMAGE, /*owner=*/1, ctx.roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [](BattleContext*, const BattleEvent& ev) {
            if (ev.target != 1) {
                return;   // 沧岚 heal 分支打到攻方的粉伤不记
            }
            ++g_b_pink_events;
            g_b_last_amount = ev.amount;
        },
        WatcherScope::TEAM);
}

void reset_probes() {
    g_pink_events = 0;
    g_last_pink_amount = -1;
    g_broken_events = 0;
    g_seg_log.clear();
}

void play_round(ScenarioBattle& b, int p0_slot = 0, int p1_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, p0_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, p1_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 战：双账消耗序 + 多段混合逐段取整 + 溢出穿透 + 免伤票负面验证 ════
    {
        ScenarioBattle b(make_party(a_attacker()), make_party(a_defender()));
        b.start();
        // 双账在账（盾=红伤账、罩=粉伤账，两笔罩高优先先耗——§3.1/§3.2）。
        b.ctx().getPet(1).shield_bank_.add(/*priority=*/0, /*quantity=*/400,
                                           kShieldSrc, /*duration=*/0);
        b.ctx().getPet(1).hood_bank_.add(/*priority=*/10, /*quantity=*/500,
                                         kHoodLowSrc, /*duration=*/0);
        b.ctx().getPet(1).hood_bank_.add(/*priority=*/50, /*quantity=*/300,
                                         kHoodHighSrc, /*duration=*/0);
        install_probes(b.ctx());
        CHECK_EQ(b.ctx().getPet(1).hp, kBigHp, "A0) 开局满血");
        CHECK_EQ(b.ctx().getPet(1).shield_bank_.total(), 400, "A0) 盾 400 在账");
        CHECK_EQ(b.ctx().getPet(1).hood_bank_.total(), 800, "A0) 罩 500+300=800 在账");

        // ── R1：红吃盾 → 粉逐段吃罩（高优先笔先耗）→ 溢出 0 ──────────────
        reset_probes();
        play_round(b);
        {
            const int drop = kBigHp - b.ctx().getPet(1).hp;
            CHECK_EQ(drop, 0, "A1) 罩 800 吃下两段粉（234+270 全被吸收）→ 掉 0");
            CHECK(b.ctx().getPet(1).shield_bank_.quantity_of(kShieldSrc) >= 298 &&
                      b.ctx().getPet(1).shield_bank_.quantity_of(kShieldSrc) <= 313,
                  "A1) ★ 红伤先吃盾：400-[87,102] ∈ [298,313]（粉伤未碰盾账——§3.1 双账分离）");
            CHECK_EQ(b.ctx().getPet(1).hood_bank_.quantity_of(kHoodHighSrc), 0,
                     "A1) ★ 高优先笔被吃光移除（端态判别位之一）");
            CHECK_EQ(b.ctx().getPet(1).hood_bank_.quantity_of(kHoodLowSrc), 296,
                     "A1) ★ 优先级消耗序端态判别：300@prio50 先耗 234、段2 续扣其 66 后"
                         "才轮到 500@prio10 → 端态 9002=296（若低优先先吃端态应为"
                         " 9002=0/9003=296——§3.2 吸收恒从最高优先级开始）");
            CHECK_EQ(g_broken_events, 1, "A1) 段2 破高优先笔 → EVENT_SHIELD_BROKEN 1 次");
            CHECK_EQ(g_pink_events, 0, "A1) 两段都没落到本体 → 不发 EVENT_TAKE_PINK_DAMAGE");
            const bool seg_ok = g_seg_log.size() == 2 &&
                                g_seg_log[0][0] == 0 && g_seg_log[0][1] == 234 &&
                                g_seg_log[0][2] == 0 &&
                                g_seg_log[1][0] == 0 && g_seg_log[1][1] == 270 &&
                                g_seg_log[1][2] == 1;
            CHECK(seg_ok, "A1) ★ FINAL 探针逐段账：[(0,absorbed=234,FIXED),(0,absorbed=270,"
                          "PERCENT)]——absorbed 记账 + tier 分型（§2.1/§2.2）");
            CHECK_EQ(g_seg_log.size(), 2u, "A1) 两段=两次独立管线（无合并结算）");
            std::cout << "  [A1] 盾余 " << b.ctx().getPet(1).shield_bank_.total()
                      << "、罩 9003=" << b.ctx().getPet(1).hood_bank_.quantity_of(kHoodHighSrc)
                      << "/9002=" << b.ctx().getPet(1).hood_bank_.quantity_of(kHoodLowSrc)
                      << "、掉血 " << drop << "（粉伤全被罩吃，盾账未动）\n";
        }

        // ── R2：罩不足 → 溢出穿透落本体，事件带实际扣血量 ─────────────────
        reset_probes();
        play_round(b);
        {
            const int drop = kBigHp - b.ctx().getPet(1).hp;
            CHECK_EQ(drop, 208, "A2) ★ 段2：罩 62 吃不尽 270 → 溢出 208 落本体（穿透）");
            CHECK_EQ(g_pink_events, 1, "A2) 只有段2 落到本体 → 发 1 次粉伤事件");
            CHECK_EQ(g_last_pink_amount, 208, "A2) ★ 事件 amount=实际扣血量 208（§2.5）");
            CHECK_EQ(b.ctx().getPet(1).hood_bank_.total(), 0, "A2) 罩账清零");
            const bool seg_ok = g_seg_log.size() == 2 &&
                                g_seg_log[0][0] == 0 && g_seg_log[0][1] == 234 &&
                                g_seg_log[1][0] == 208 && g_seg_log[1][1] == 62;
            CHECK(seg_ok, "A2) ★ 段1 全被罩吃(0,234)；段2 罩只参与 62 → (208,62)");
            CHECK(b.ctx().getPet(1).shield_bank_.quantity_of(kShieldSrc) >= 196 &&
                      b.ctx().getPet(1).shield_bank_.quantity_of(kShieldSrc) <= 226,
                  "A2) 盾再吃一轮红 [196,226]=400-2×[87,102]（粉伤两回合均未碰盾账）");
            std::cout << "  [A2] 掉血 " << drop << "（溢出穿透）、事件 amount="
                      << g_last_pink_amount << "、罩余 " << b.ctx().getPet(1).hood_bank_.total()
                      << "\n";
        }

        // ── R3：免伤票在场（红）→ 红被挡、粉照掉（负面验证）────────────────
        const int hp_after_r2 = b.ctx().getPet(1).hp;
        reset_probes();
        b.ctx().grant_immunity(/*owner=*/1, ImmunityType::DAMAGE, coverage_all(),
                               /*anomaly_mask=*/0, /*duration_rounds=*/0,
                               /*source_id=*/0, /*soul_immunity=*/false,
                               EffectScope::ON_STAGE, /*counts=*/2,
                               /*source_effect_id=*/9994);
        CHECK(b.ctx().is_immune(1, ImmunityType::DAMAGE, State::BATTLE_FIRST_ATTACK_DAMAGE),
              "A3) 前置：红伤票在账");
        play_round(b);
        {
            const int drop = hp_after_r2 - b.ctx().getPet(1).hp;
            CHECK_EQ(drop, 504, "A3) ★ 红伤被票挡下 → 本回合掉血全为粉伤 234+270=504"
                                "（§五 推论 1：粉伤不进主伤害管线，红票不代用）");
            CHECK_EQ(g_pink_events, 2, "A3) 两段粉均落本体（免伤票在场照掉）");
            CHECK(b.ctx().is_immune(1, ImmunityType::DAMAGE, State::BATTLE_FIRST_ATTACK_DAMAGE),
                  "A3) ★ counts=2 红票只被红伤消费 1 张仍在场——粉伤结算全程**未碰**红票"
                  "（两条管线互不可代用的账面铁证）");
            std::cout << "  [A3] 红票在场：掉血 " << drop << "（全为粉伤）、粉事件 "
                      << g_pink_events << " 次、红票仍剩 1 张\n";
        }

        // ── 全场总账（多段取整跨回合精确累计）────────────────────────────
        CHECK_EQ(b.ctx().getPet(1).hp, kBigHp - (0 + 208 + 504),
                 "A4) ★ 全场体力精确：掉 712 = R1 0 + R2 208 + R3 504（逐段取整无漂移）");
        const int fixed_resist_math = 333 - 333 * 30 / 100;    // 234（逐段整数除法）
        const int percent_resist_math = 300 - 300 * 10 / 100;  // 270
        CHECK_EQ(fixed_resist_math + percent_resist_math, 504,
                 "A4) 两段各按**各自**抗性通道取整（333×30%、300×10%）——合并成一段"
                 " FIXED 633 只会是 633−189=444 ≠ 504（§2.6 逐段取整可观测）");
        std::cout << "  [A4] 全场掉血 712 精确；逐段取整 234+270=504 ≠ 合并 444\n";
    }

    // ══ B 战：2343 护盾门槛（quantity_of 口径）端到端 ══════════════════════
    {
        // 沧岚持真 2343 魂印（登记程序链：early on_enter 发 400 登场盾 + CAP 注册）。
        ElfPet canglan = make_pet(2, "沧岚", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                  /*speed=*/100, /*atk=*/1, /*hp=*/4000,
                                  SoulMark(kCanglanMark, "怒涛·沧岚", "沧岚"));
        ElfPet attacker = make_pet(1, "粉伤攻方B", {kPaiDa, kGaoSu, kPaiDa, kPaiDa, kPaiDa},
                                   /*speed=*/300, /*atk=*/300, kBigHp,
                                   make_pink_600_mark());
        ScenarioBattle b(make_party(attacker), make_party(canglan));
        b.start();
        // ★ 真实 INIT 的登场盾（early on_enter 随首个回合的 ENTER_EXIT_STAGE 桶
        //   发放，非手工 add——051 P5/7790 A1 同样在回合流后查账）。B0 的证明
        //   折进 R1：q1 ∈ [298,313] 只有「INIT 满盾 400 先在账、再被真实红伤消耗
        //   [87,102]」才能成立（若无盾：红伤直落本体、CAP 无盾失效 → 粉 600 全落，
        //   本回合掉血会是 [687,702] 而非 =q1）。
        const ShieldBank& bank = b.ctx().getPet(1).shield_bank_;
        install_b_probe(b.ctx());

        // ── R1：真实红伤先消耗盾 → ROUND_END 粉 600 被 CAP 钳到盾现值 ──────
        int hp_before = b.ctx().getPet(1).hp;
        g_b_pink_events = 0;
        g_b_last_amount = -1;
        play_round(b);
        {
            const int q1 = bank.quantity_of(kCanglanMark);
            CHECK(q1 >= 298 && q1 <= 313,
                  "B0+B1) ★ 真实 INIT 登场盾 400 在账并被红伤消耗：现值 ∈ [298,313]"
                      "（=400-[87,102]），实测 " + std::to_string(q1));
            CHECK_EQ(g_b_pink_events, 1, "B1) 粉落体事件恰 1 次（真的结算到本体）");
            CHECK_EQ(g_b_last_amount, q1,
                     "B1) ★ 粉 600 实落 = quantity_of(2343) **现值**（CAP 实时求值；"
                     "落体事件 amount=现值——钳值跟着真实红伤消耗后的账走，不是缓存 400）");
            std::cout << "  [B1] 盾现值 " << q1 << "、粉落事件 amount=" << g_b_last_amount
                      << "、本回合净掉血 " << (hp_before - b.ctx().getPet(1).hp)
                      << "（2343 回合末 heal 分支抵平——R1 flag 未置位走 heal，如实记档）\n";
        }

        // ── R2：攻方属性技轮空（槽 1 = 高速移动，无人消耗盾）→ 钳值不变 ─────
        const int q1 = bank.quantity_of(kCanglanMark);
        hp_before = b.ctx().getPet(1).hp;
        g_b_pink_events = 0;
        g_b_last_amount = -1;
        play_round(b, /*p0_slot=*/1, /*p1_slot=*/0);
        {
            const int q2 = bank.quantity_of(kCanglanMark);
            CHECK_EQ(q2, q1, "B2) 属性技轮空：盾现值跨回合不变（无人消耗）");
            CHECK_EQ(g_b_last_amount, q2,
                     "B2) 粉 600 再落 = 现值（CAP 只钳不耗盾——051 P5 同款口径）");
            std::cout << "  [B2] 盾现值 " << q2 << "、粉落 amount=" << g_b_last_amount
                      << "、净掉血 " << (hp_before - b.ctx().getPet(1).hp) << "\n";
        }

        // ── R3：再打一轮红（槽 0）→ 粉实落仍等于当时的盾现值（两分支同式成立）
        hp_before = b.ctx().getPet(1).hp;
        g_b_pink_events = 0;
        g_b_last_amount = -1;
        play_round(b);
        {
            const int q3 = bank.quantity_of(kCanglanMark);
            CHECK_EQ(g_b_last_amount, q3,
                     "B3) ★ 第三轮粉实落仍 = 当时 quantity_of 现值——若前序回合分支发的"
                     "红伤票挡下红伤则 q3==q1（盾不再减）；若红再吃盾则 q3=q1-[87,102]；"
                     "两分支下「实落=现值」等式同成立（实时求值的分支鲁棒证）");
            CHECK(q3 <= 400, "B3) 现值不高于初始 400（盾从未被 CAP 补回）");
            std::cout << "  [B3] 盾现值 " << q3 << "、粉落 amount=" << g_b_last_amount
                      << "、净掉血 " << (hp_before - b.ctx().getPet(1).hp) << "\n";
        }
    }

    std::cout << "scenario_7833 PASS（S20 穿测②：08 章粉伤管线+双账口径真实战斗流端到端——"
                 "双账分离/罩优先级消耗序/absorbed 记账/溢出穿透/免伤票负面验证/多段混合"
                 "逐段取整/2343 门槛现值实时求值，串成完整 3 回合对局）" << std::endl;
    return 0;
}
