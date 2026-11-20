// J 组 J319 · 元模板窗族 —— effect 730（1 宏/9 技引用）+ 2190 查证记档（2026-11-03，worktree ../agents/j319）
//
// ══ 730「下回合起，{0}回合内每回合{1}」（effect_info args_num=7；side_effect.arg_count=7 双表一致）══
// {1} 吞 6 参 = **语义序六槽向量（攻,防,特攻,特防,速,命中）**，映射引擎槽 kStatMap{0,2,1,3,4,5}
// （stat_dispel.cpp kStatMap / 477 / J3 1823 / J48 732 同表）。⚠️ 属性排列双序坑（速查 §三）：
//   按**引擎序**直读会把防/特攻读反——4399 文本交叉验证定案（见下表，万物生 [0,1,0,1,0,0]
//   =「防御和特防等级+1」：语义序 idx1→引擎槽2 防 ✓；若按引擎序 idx1=特攻 ✗）。
//
// 9 技引用切片 × 4399 文本逐位互证（2026-11-03，切片=side_effect.arg_count 链上按位分摊）：
//   23906 无法无天   [3,0,0,1,0,1,0] 圣瞳/圣仁之瞳·天蛇太祖：「3回合内每回合特攻+1、速度+1」
//                    → idx2=特攻 idx4=速 ✓
//   24551 盛典进行曲 [3,1,1,0,1,0,0] 乐乐熊：「下3回合，每回合攻击+1，双防+1」
//                    → idx0=攻 idx1=防 idx3=特防 ✓
//   24559 破碎星域   [3,1,0,0,0,1,0] 弗里德/不羁·弗里德：「3回合内每回合攻击，速度+1」✓
//   24887 龙神守护   [3,1,0,0,0,1,0] 龙翼战神：「3回合内，每回合攻击+1，速度+1」✓
//   24943 怒波水柱   [2,0,1,0,1,0,0] 艾尔：「2回合内，每回合防御+1、特防+1」✓
//   24949 冲云流星   [3,1,0,0,0,1,0] 英灵剑圣/跃迁狂剑·兰斯洛特：「3回合内，每回合自身攻击+1、速度+1」✓
//   27170 电脉充能   [2,1,0,0,0,1,0] TSP：「下回合起，2回合内每回合攻击、速度等级+1」
//                    ——载体文本与 effect_info 模板逐字同构，定案模板权威 ✓
//   27526 万物生     [4,0,1,0,1,0,0] 魔花仙灵：「4回合内每回合防御和特防等级+1」✓
//   27874 魂源离散   [2,0,0,1,0,1,0] 哀·拉伯克：「2回合内每回合特攻+1、速度+1」✓
// 9/9 全部自洽：首参=回合数 {0}，后 6 参=语义序六槽 {1}；全部为**自身**增益（正增量）。
//
// 落法：
//   - 「下回合起」= 官方窗口规则 NextRounds（本回合不算，先/后手都 [R+1,R+N]；
//     round_effect_start_round 注释官方通用规则，J184 2232 同口径）→ 不做施放回合内联。
//   - 逐回合 tick 走 **ROUND_END 桶**（J73 409 直系：ROUND_START 桶曾被实测每回合
//     两遍跳、ROUND_END 每回合只跑一次；409 形已全绿合入）。NextRounds(N) 注册后
//     施放回合 ROUND_END 也会跑一次 tick（isExpired 按 registered_round_=R+1 计，
//     R-(R+1)<N 不算过期）→ tick 内带回合界守卫（roundCount∈[r0+1, r0+N] 才应用，
//     409 同款"tick 内自判，不依赖窗口语义"），施放回合那次空跑被守卫跳过。
//   - 六槽应用（1823 同构分支）：delta>0 → stat_change（自身提升/自降**不吃免弱**，
//     battle_primitives.h stat_change 分工注释）；delta<0 → 亦走 stat_change（1823
//     dst==owner 分支同款：窗为自身向，自降不是"对手施加的弱化"）。delta==0 跳过。
//     ⚠️ 记档：9 载体全为正增量自增益，负值分支无载体实证——按 1823 自身向约定落，
//     若官方出现"负值=降对手"的 730 载体再改分支（ Majority reading 2026-11-03）。
//   - ±6 钳制在 stat_change 原语内；本体/视图一并同步（手抄 clamp 会致视图分裂）。
//
// ══ 2190「{0}」—— 查证结论：哑模板，记档转单行池（不产码）══
// effect_info args_num=1、info 字面就是「{0}」纯参；side_effect.arg_count 已由 R1 补丁
// 0→1（tools/db_patches/r1/2190_0to1.sql，管线重建后必须重放）。13 技引用切片恰为
// [0]..[12] 连续 13 值——形似"元程序号"（每技能一份独立程序），但程序本体不在库中
// （custom_effect_programs 无 2190 行）。4399 文本必查（2026-11-03 全图鉴扫）：
//   28745 自启动核心（天阶机器人）：「智能运转的启动部件」——**无效果文本**；
//   28764 月轮·极阴 / 37429 日轮·极阳（浮掠双刃·卓尔布）：携带型被动（霜浮/灼掠之刃
//     触发概率 ±50%）——**非行动效果**；
//   28780 十玄释 / 28781 百法拜 / 37458 孑身诫 / 37459 千秋虔（无为觉者）：四条互异的
//     多子句复合（免疫窗/吸取窗/PP 操作/臣服…）；
//   29133 征天夜陵 / 29134 殷痕血幕 / 29211 浩域森罗 / 29245 噬古魔怀 / 38064 神暮 /
//     38186 诉与孤风：DB 无归属精灵（mon_id=0、learnable/hide 均无）、4399 图鉴未收录。
// → 13 载体 4399 文本**互不相同**（6 无文本 + 2 携带被动 + 4 互异复合 + 1 纯叙述），
//   每载体语义独立、无法按宏批产 → **看板行第一分支坐实：哑模板（文本缺失靠载体自述），
//   记档转单行池**。与 batch_b_skills.cpp 前人结论（2026-09，当时 arg_count=0、仅见
//   无为觉者 4 技）一致且证据面扩展到 13 技。
//
// 动工前置三查（2026-11-03）：①内核 grep 730/2190 无行为硬编码（effect_meta.cpp 两 id
//   零条目）②custom_effect_programs/overrides 无 730/2190 声明 ③moves_lib/soul_lib 并集
//   无 730/2190 注册——真缺口（2190 按上记档不立）。
// 先例：J73 409（NextRounds+ROUND_END tick+回合界守卫）、J3 1823/J48 732（kStatMap
//   语义序六槽+正负分支）、J184 2232/2492（每回合重写形——布尔位重写场景适用，本族
//   为累积型 stat_change 不适用重写语义，记档）。
// 场景回归：test/scenario/scenario_7670_meta_template_window.cpp（号段 7670-7679）。

#include "common.h"

namespace {

// 语义序（攻,防,特攻,特防,速,命中）→ 引擎能力槽（J3 1823 / J48 732 kStatMap 同表）。
constexpr int kStatMap[6] = {0, 2, 1, 3, 4, 5};

// ── 730 的回合末 tick —— payload={owner, rounds, r0, 六槽×6}（int_count=9）──
EffectResult j319_730_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 9 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[1];
    const int r0 = args.int_args[2];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // 回合界 [r0+1, r0+rounds]（「下回合起」；tick 内自判，不依赖窗口语义——409 同款。
    // NextRounds 注册后施放回合 ROUND_end 的那趟 tick 由此守卫跳过）。
    if (ctx->roundCount <= r0 || ctx->roundCount > r0 + rounds) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < 6; ++i) {
        const int delta = args.int_args[3 + i];
        if (delta == 0) {
            continue;
        }
        // 自身向窗：正增量提升 / 负增量自降都走 stat_change（不吃免弱，1823 dst==owner 分支）。
        (void)g_core->stat_change(ctx, owner, kStatMap[i], delta);
    }
    return EffectResult::kOk;
}

// ── 730 —— 下回合起，{0}回合内每回合{1}（{1}=语义序六槽）──
// int_args = [owner, target, rounds, 六槽×6]（args_num=7；target 未用——窗为自身向）。
EffectResult effect_730_per_round_sixslot_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 730, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 9, 730, args,
                     "730 布局=[owner,target,rounds,六槽×6]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 730, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 730, args, "需要 回合数 > 0");
    // 「下回合起」→ NextRounds 窗 [R+1, R+N]，ROUND_END 桶逐回合 tick（409 直系）。
    // 不做施放回合内联；payload 六槽原样透传。
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_END, owner, 730, rounds,
                                {owner, rounds, ctx->roundCount, args.int_args[3],
                                 args.int_args[4], args.int_args[5], args.int_args[6],
                                 args.int_args[7], args.int_args[8]},
                                &j319_730_tick);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）────────────────────────────────────────────────────
// 2190 不注册：哑模板记档转单行池（见文件头查证结论）。
SKILL_EFFECT(730, effect_730_per_round_sixslot_window)
