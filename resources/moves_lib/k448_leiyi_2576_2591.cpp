// K448 · 万霆神律·雷伊 4946 残段 —— 2576「{0}回合内自身受到的固定伤害、百分比伤害
// 不超过{1}点」（args_num=2）+ 2591「未击败对手则恢复自身所有技能{0}点PP值且1回合内
// 造成的攻击伤害提升{1}%」（args_num=2）（2026-11-10，worktree k448 接续段；场景
// test/scenario/scenario_8360_leichen_engine.cpp T16+ 腿）
//
// ══ 载体切片（冻结库 a15eca92 现查 2026-11-10；与 k451/HUB 共库同指纹）══
//   2576 [4,70]：29455 雷·万鸣天锻（4946 learnable）链 [191,2576,1683,1200,1834]
//     args=[4,4,70,4,4,100,0,1,300,70] → 191[4]✅(J?) / 2576[4,70] 本单 / 1683[4]✅ /
//     1200[4,100,0]✅ / 1834[1,300,70]❌ 未实现（静默跳过，红伤 cap+反弹另族）。
//   2591 [1,100]：38538 雷·神天明闪（4946 extra 第五技）链 [760,2573,1885,2574,2591,2017]
//     args=[3,100,19,3,1,100] → 760[]✅ / 2573[]✅(k448_leilian) / 1885[3,100,19]✅ /
//     2574[3]❌（盖亚段保留，K449 域） / 2591[1,100] 本单 / 2017[]✅。
//   2570 [0]（38536/38624/38630 链尾）：「技能无效时，消除对手回合类效果并恢复自身
//     所有体力」——SKILL_INVALID 分支直执行（2593 同形）。✅ 内核 case 2570 已获
//     a31-orch 授权（2026-11-10 追加段）并注册；消回合=break_round_effects 真原语、
//     回满=heal_amount(max)，两动作并列独立（免断不挡回血/封回血不挡消回合）。
//
// ══ 实现形态（同形套路照抄先例）══
// · 2576 = **PinkDamagePhase::CAP**（pink_damage_pipeline.h:133"受到的粉伤不超过X点"，
//   沧岚 canglan_pink.cpp:254 同阶段直系）：SKILL_EFFECT 时点装 CAP 条目（幂等旗，
//   单条目常驻）+ 窗末回合台账（plugin_storage 单键，重复用技顺延——"{0}回合内"
//   InRounds 计法：授予回合为第 1 回合，active while roundCount <= grant+rounds-1）。
//   回调内实时判窗（管线条目"有效性回调里实时判"纪律）+ 按位钳 final=min(final,cap)。
//   按段钳制（粉伤逐段独立结算——"每段不超过"字面口径）。
// · 2591 = **897 AFTER_ACTION 桥**（k448_leilian.cpp 2571/2572 同构：双侧探针
//   duration=1，bucket_acting_side 过滤——AFTER_ACTION 时伤害已落地、未击败可知）：
//   未击败（对手存活）→ ① pp_restore(自身, {0}, only_empty=false)（"恢复自身所有
//   技能{0}点PP值"全槽原语，clamp ≤ maxPP）；② "1回合内造成的攻击伤害提升{1}%" =
//   register_next_rounds_effect(ROUND_START, NextRounds 1 回合) 窗——窗内 ROUND_START
//   写 ws.damage_add_pct += {1}%（回合类效果每回合重写纪律，battleWorkspace.h:305），
//   窗 = 下一起始的整 1 回合（触发时自身本回合已出手，"1回合内"取下回合整窗；
//   裁决记档：若官方口径含当回合剩余，只改窗口起点枚举）。已击败 → 门闭（slot9=1）。
// · 两宏均 SKILL_EFFECT 时点、行参布局 [owner, opp, 行参…]（loader 注入前缀）。
//
// ══ 三查（2026-11-10 冻结库 a15eca92）══
// ① 内核 grep (^|[^0-9.])(2576|2591)([^0-9.]|$) src/ include/ 零命中。
// ② effect_info 2576 args_num=2 / 2591 args_num=2 = 切片消耗 ✓；custom_* 四表零命中。
// ③ 全库注册并集无两 id（resources/ test/ examples/ SKILL_EFFECT 零命中；跨 worktree
//   k449/k451 零命中）——真缺口。registry moves=1522 基（k451 已 +2）不含两 id。
// ④ parser：两模板 { 前有字 → parse_effect_unit 不命中 → 纯惰性注册。
// 存储键段（leichen_runtime.h 已登记）：1025760000/1025761000/1025761100/
// 1025910000 —— check_storage_keys.py 全库零占用。
//
// ══ 审计槽布局（plugin_storage，键 = base + owner*32 + slot）══
//  2576（1025760000）：slot 0 行参 rounds / 1 行参 cap / 2 窗末回合 / 3 钳制次数 /
//    4 EVENT_BREAK 击杀次数（同步杀台账审计） / 8 累计使用 / 9 跳过原因（1=无效参）
//  2591（1025910000）：slot 0 行参 pp / 1 行参 pct / 2 pp 恢复返回量 / 3 增伤窗起点
//    回合 / 4 增伤写手执行次数 / 5 未击败门进入次数 / 8 累计使用 / 9 跳过原因
//    （0=无 / 1=已击败（门闭））
// ══ 变异测试（本段 MA-MJ，2026-11-10 全部真转红：改→显式 deploy 重编→8360
//    必红→字节级还原（cmp）→重编+deploy→回绿）══
//   MB 2576 钳制体禁用 → T17 红 / MC 2591 摘 pp_restore → T19 红 /
//   ME 2591 NextRounds→InRounds（写手赶不上 R2 ROUND_START）→ T19 红 /
//   MF 2591 pct 100→50 → T19 ×2 等式红 / MH 2576 摘窗检查 → T17 窗外腿红 /
//   MI 2591 摘增伤窗注册 → T19 红 / MJ 2576 摘 EVENT_BREAK watcher → T17a
//   alive 台账腿红（-1≠1）。（MA/MD"摘 stage 标记门"与 MG"摘 target 检查"
//   三支经实测为**无观察面变异**已撤：窗失效由 ON_STAGE scope+epoch 内建覆盖、
//   粉伤管线只跑受击方桶——check 本身即机制实证，记档。）

#include "common.h"

#include <effects/pink_damage_pipeline.h>

#include "../leichen_runtime.h"

namespace {

// ── 审计/存储小工具（k448_leilian 同构私有副本）────────────────────────────
constexpr int k2576AuditBase = 1025760000;
constexpr int k2576WinBase = 1025761000;    // +owner：窗末回合（含；重复用技顺延）
constexpr int k2576ArmBase = 1025761100;    // +owner：CAP 条目安装旗
constexpr int k2576CapLedger = 1025761200;  // +owner：cap 台账（回调实时读——不捕获）
constexpr int k2576AliveBase = 1025761300;  // +owner：窗活性 tick 台账（==roundCount 即活）
constexpr int k2591AuditBase = 1025910000;  // +owner*32+slot（跨距见 audit_key_of 注）

void set_st(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

int get_st(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? def : any_int_or(it->second, def);
}

void bump_st(BattleContext* ctx, int key) {
    set_st(ctx, key, get_st(ctx, key, 0) + 1);
}

// 审计键跨度 32（K448 验收修正 2026-11-10）：与 leilian/引擎统一，防 slot 扩展段
// 与邻 owner 槽 0 相撞（旧 *16 布局 owner0-slot16 == owner1-slot0）。
int audit_key_of(int base, int owner, int slot) {
    return base + owner * 32 + slot;
}

// 换宠失效机制（2026-11-10 实测定案）：register_pink_damage_effect / register_skill_effect
// 默认 **ON_STAGE scope + pipeline_valid_id epoch**（battleContext.h:1440-1442）——
// perform_switch 递增 epoch → 条目自动惰性失效，**零插件代码即满足**"watcher/epoch
// 随上下场一起重置"的用户口径。/arm 旗故而必须落在 on_stage_storage（per-pet，
// 换宠即清）——side 级 plugin_storage 旗会让换宠后的新宠失去重装机会（真 bug，T18 实证）。

// AFTER_ACTION 桶的当前行动方（k448_leilian 同款私有副本）。
int leiyi_bucket_acting_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

// ── 2576 CAP 回调（单条目常驻；cap/窗活性全走台账——不捕获首次参数）──────────
void leiyi_2576_install(BattleContext* ctx, int owner) {
    // 安装旗落 **on_stage_storage**（per-pet，perform_switch 自动清）：换宠后条目随
    // epoch 失效，旗若留 side 级会让回场重施失去重装机会（T18 实测发现）。
    auto& st = ctx->getPet(owner).on_stage_storage;
    if (st.count(kLeiYiCapArmKey)) {
        return;   // 本宠在场期内已安装（单条目；窗口/cap 经台账刷新）
    }
    st[kLeiYiCapArmKey] = 1;
    ctx->register_pink_damage_effect(
        PinkDamagePhase::CAP, owner,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            // 窗活性实时判（K448 验收修正：活性台账由窗锚 tick 维护 + EVENT_BREAK
            // 同步击杀，滞后容差 alive >= roundCount-1——CHOOSE 停泊点 roundCount
            // 已预进位、新回合 ROUND_START 未跑，容差恰好覆盖「上轮 tick 过、本轮
            // 待 tick」。break_round_effects：内核 emit EVENT_BREAK（仅 SUCCESS 发，
            // IMMUNE/NO_EFFECTS 不发——T24/T27 已证）→ 本文件 break watcher 当场
            // 写 alive=-1 → cap 即关，同回合后续行动不再吃钳制；免断锚存活 →
            // tick 继续 → cap 保持。与 pipeline_valid_id 的换宠失效是两层，互不替代）。
            if (get_st(c, k2576AliveBase + owner, -1) < c->roundCount - 1) {
                return;   // 窗外（锚被消回合断 / 已到期）
            }
            if (c->roundCount > get_st(c, k2576WinBase + owner, -1)) {
                return;   // 窗末回合守卫
            }
            // cap 实时读台账（K448 验收修正：重施不同 {1} → 台账刷新 → 即刻生效，
            // 不再捕获首次 cap）。
            const int cap = get_st(c, k2576CapLedger + owner, -1);
            PinkDamageResolved& r = c->resolvedPink;
            if (cap <= 0 || r.target != owner || r.final <= 0) {
                return;
            }
            if (r.final > cap) {
                r.final = cap;
                bump_st(c, audit_key_of(k2576AuditBase, owner, 3));
            }
        });
    // EVENT_BREAK 同步击杀（K448 验收修正：tick 滞后容差会在「断的当回合」残留
    // cap 一个回合界；真实对局 break 事件在本状态桶尾 drain——watcher 当场杀台账，
    // 同回合后续粉伤即恢复无钳。任何对本方的 SUCCESS 断回合都强过期本锚
    // （invalidate_all_round_effects 全桶 epoch 作废）→ watcher 杀台账与锚死亡
    // 精确同义，无误杀面）。battle 级常驻 + 战斗级一次性旗（换宠回场重施再走
    // install 时不得重复注册——TEAM watcher 不随 epoch 失效）。
    constexpr int k2576BreakArmBase = 1025761400;   // +owner：break watcher 接线旗
    if (!ctx->plugin_storage.count(k2576BreakArmBase + owner)) {
        ctx->plugin_storage.emplace(k2576BreakArmBase + owner, 1);
        ctx->event_center_.register_watcher(
            EventType::EVENT_BREAK, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.target != owner) {
                    return;
                }
                set_st(c, k2576AliveBase + owner, -1);
                bump_st(c, audit_key_of(k2576AuditBase, owner, 4));
            },
            WatcherScope::TEAM);
    }
}

// ── 2576 窗锚 tick（ROUND_START，duration=rounds 的**回合类条目**）────────────
// 进入 has_round_effects 可清集合：break_round_effects 对窗宿主强制过期锚 →
// tick 停 → 活性台账滞后 → CAP 失效；免断票命中则锚存活、cap 保持。到期自然停。
EffectResult leiyi_2576_window_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = a.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    set_st(c, k2576AliveBase + owner, c->roundCount);
    return EffectResult::kOk;
}

// ── 2576 本体 ──────────────────────────────────────────────────────────────
EffectResult effect_2576_pink_cap_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2576, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2576, args,
                     "2576 布局=[owner,target,rounds,cap]（args_num=2）");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int cap = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 2576, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && cap > 0, 2576, args, "需要 rounds/cap > 0");
    // 窗末回合（InRounds：授予回合为第 1 回合；roundCount 0-based → 含 grant+rounds-1）。
    const int win_end = ctx->roundCount + rounds - 1;
    set_st(ctx, k2576WinBase + owner, win_end);
    set_st(ctx, k2576CapLedger + owner, cap);   // cap 入台账（回调实时读）
    // 授予回合活性即刻成立（本回合 ROUND_START 已过 → 锚补不到授予回合）。
    set_st(ctx, k2576AliveBase + owner, ctx->roundCount);
    set_st(ctx, audit_key_of(k2576AuditBase, owner, 0), rounds);
    set_st(ctx, audit_key_of(k2576AuditBase, owner, 1), cap);
    set_st(ctx, audit_key_of(k2576AuditBase, owner, 2), win_end);
    leiyi_2576_install(ctx, owner);
    // 窗锚（每 cast 都注册；duration=rounds → isRoundEffect → has_round_effects
    // 可清集合成员；重施旧锚自然到期、双锚幂等写同一台账）。
    {
        Effect e(2576, 0, owner, -1, EffectArgs(std::vector<int>{owner}),
                 &leiyi_2576_window_tick);
        auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, owner,
                                                     /*duration=*/rounds, ctx->roundCount);
        ce->source_id_ = 2576;
        ctx->register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
    }
    bump_st(ctx, audit_key_of(k2576AuditBase, owner, 8));
    return EffectResult::kOk;
}

// ── 2591 增伤写手（InRounds 即时写；下回合 ws reset 自动清窗）────────────────
EffectResult leiyi_2591_boost_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = a.int_args[0];
    const int pct = a.int_args[1];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }

    c->ws.damage_add_pct[owner] += static_cast<float>(pct);
    bump_st(c, audit_key_of(k2591AuditBase, owner, 4));
    return EffectResult::kOk;
}

// ── 2591 AFTER_ACTION 探针：未击败 → 全槽 PP+{0} + 本回合剩余增伤窗 ──────────
EffectResult leiyi_2591_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 4) {
        return EffectResult::kOk;
    }
    const int me = a.int_args[0];
    const int pp = a.int_args[2];
    const int pct = a.int_args[3];
    if (me < 0 || me > 1 || leiyi_bucket_acting_side(c) != me) {
        return EffectResult::kOk;   // 只在自身用技的 AFTER_ACTION 结算
    }
    const int opp = 1 - me;
    set_st(c, audit_key_of(k2591AuditBase, me, 8),
           get_st(c, audit_key_of(k2591AuditBase, me, 8), 0) + 1);
    if (opp < 0 || opp > 1 || c->getPet(opp).hp <= 0) {
        set_st(c, audit_key_of(k2591AuditBase, me, 9), 1);   // 已击败（门闭）
        return EffectResult::kOk;
    }
    bump_st(c, audit_key_of(k2591AuditBase, me, 5));
    // ① 恢复自身所有技能 {pp} 点 PP（全槽原语，clamp ≤ maxPP；返回实际总量）。
    int restored = 0;
    if (g_core && g_core->pp_restore) {
        restored = g_core->pp_restore(c, me, pp, /*only_empty=*/false);
    }
    set_st(c, audit_key_of(k2591AuditBase, me, 2), restored);
    // ② "1回合内造成的攻击伤害提升{pct}%"：**InRounds(1) = [R, R]**（窗口家族默认档
    //    "先出手本回合就结算一次"，495 条模板族口径）——即时写 ws.damage_add_pct：
    //    触发后同回合的额外行动（追加行动/多段后手）读同一 ws 槽立即吃到增幅；
    //    下回合 ws.reset 自动清 → 恰好一回合（K448 验收修正：旧 NextRounds 取
    //    "下回合整窗"无机制依据，且漏同回合额外行动半区）。
    //    仲裁注记：若官方实测为"下回合"窗，只改本写点为 NextRounds 注册（一处）。
    leiyi_2591_boost_tick(c, EffectArgs(std::vector<int>{me, pct}));
    set_st(c, audit_key_of(k2591AuditBase, me, 3), c->roundCount);   // 窗起点=R（当回合）
    (void)pp;  // pp 已消费于 pp_restore
    return EffectResult::kOk;
}

// ── 2591 本体 ──────────────────────────────────────────────────────────────
EffectResult effect_2591_undefeated_pp_boost(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2591, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2591, args,
                     "2591 布局=[owner,target,pp,pct]（args_num=2）");
    const int owner = args.int_args[0];
    const int pp = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 2591, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(pp > 0 && pct > 0, 2591, args, "需要 pp/pct > 0");
    set_st(ctx, audit_key_of(k2591AuditBase, owner, 0), pp);
    set_st(ctx, audit_key_of(k2591AuditBase, owner, 1), pct);
    // 897 AFTER_ACTION 桥（k448_leilian 同构：双侧探针 duration=1）。
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(2591, 0, owner, -1, EffectArgs(args.owned_int_args), &leiyi_2591_probe);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 2591;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 2570 —— 技能无效时，消除对手回合类效果并恢复自身所有体力（K448 接续授权段）──
// int_args = [owner, target]（args_num=0）。SKILL_INVALID 分支直执行（2593 同款，
// 内核 case 2570 已授权）：首次 MISS/SEALED 也当场执行，无事件桥首用缺口。
// 两动作按原文并列独立：消回合走 break_round_effects 真原语（免断=IMMUNE 不清不
// 发事件 / 无可清=NO_EFFECTS 不发事件 / 成功=invalidate+clear_round_type+EVENT_BREAK）；
// 回满走 heal_amount(缺口=max)——免断不挡回血、封回血不挡消回合（各由原语自身
// 检查链负责：HEAL_BLOCK 挡 heal_amount、BREAK 票挡 break_round_effects）。
EffectResult effect_2570_invalid_break_fullheal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2570, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2570, args,
                     "2570 布局=[owner,target]（args_num=0 仍带双参）");
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int target = 1 - owner;
    if (g_core && g_core->break_round_effects) {
        (void)g_core->break_round_effects(ctx, target);
    }
    if (g_core && g_core->heal_amount) {
        (void)g_core->heal_amount(ctx, owner, max_hp_of(ctx, owner));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2576, effect_2576_pink_cap_window)
SKILL_EFFECT(2591, effect_2591_undefeated_pp_boost)
SKILL_EFFECT(2570, effect_2570_invalid_break_fullheal)
//   （default_branch_for_effect case 2570 → SKILL_INVALID，2593 同款；表项授权 =
//     a31-orch 2026-11-10，src/entities/skills.cpp 唯一获准写点，仅此一 case。）
