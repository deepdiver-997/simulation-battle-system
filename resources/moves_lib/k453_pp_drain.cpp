// K453 · PP 吸取族 —— 2564×1（2026-11-10，worktree ~/.agent-workspaces/workspaces/sbs-local/k453）
//
// 家族语义（effect_info 官方模板即权威）：
//   2564（args_num=1）"吸取对手所有技能{0}点PP值"
//
// ══ 口径定案（2026-11-10 主会话裁定引用既有用户口径）══════════════════════════
//   "吸取……PP值"**无去向 = 纯扣除，不转给自己**：
//   · 官方名词解释（reference/scrape/out/all_posts_final.jsonl 行 127）
//     「叵：不受PP值限制且命中时吸取对手所有技能1点PP值」——无受益半边；
//   · 语料里带受益方的吸取必写明去向（孤烛灯使「吸取对手所有技能2点PP值恢复至
//     己方上只精灵」）→ 无去向 = 纯扣除（2026-09-23 批 8 修正轮 #3，
//     docs_local/docs/06-交接/魂印与技能机制-交接.md:1174-1177 定案，
//     当时的"一来一去"口径同条作废；场景 103 B2 断言翻转）；
//   · 2564 官方文本同形（吸取对手所有技能{0}点PP值）且无去向；biligame wiki
//     （技能:38505）把该条款类目标签为「减PP」，无恢复类标签；
//   · 对照：带去向的 2090「吸取对手第五技能剩余的PP值」是异文，其实现
//     （kongyuan_zhishi.cpp）按"一来一去"落且直写 pp——那是作废旧口径的存量，
//     **不照抄**（新代码走原语纪律）。
//
// 实现 = pp_reduce(target, {0}) 原语（include/primitives/battle_primitives.h:757-767）：
//   · 全槽 pp -= amount（clamp ≥ 0；pp == -1 无限 PP 槽跳过）；
//   · 每个实际变化的槽逐槽 emit EVENT_PP_REDUCED（PP 监听族的依赖面）；
//   · **不另造 PP 存储、不直写 skills[i].pp**（core_api.h:118-125 填充纪律）。
//
// 时序（引擎内生，无需自造；场景两序各断言）：
//   · 本效果在 SKILL_EFFECT 时点桶执行（effect_register_state default，
//     src/entities/skills.cpp:182-183——未列 id 全落 SKILL_EFFECT，无路由改动）；
//   · 用技消耗 PP 在 AFTER_ACTION（src/fsm/battleFsm.cpp:1981/2123
//     consume_selected_skill_pp）→ 先手方吸取打到对手"将用未耗"的 PP 值
//     （对手本回合消耗在其 AFTER_ACTION 才发生），后手方吸取读到已耗后的值。
//
// 载体切片（unity 库 json_each 精确匹配；side_effect 表 2440-2621 段无行，
//   arg_count 权威回落 effect_info.args_num，顺序切分全载体零剩余）：
//   2564 [点数]：星光·能源吸收 38505（星光·雷纳多 4937，learnable lv81）
//   [948, 894, 2564] | [19, 250, 2] → 948 吸强[19] / 894 吸血 250 / 2564 扣 2
//   （wiki 原文三条款逐字对上：吸强瘫痪 / 吸 250 体力翻倍 / 吸所有技 2 点 PP）。
//
// 动工前置三查（2026-11-10 本 worktree 复核）：
//   ①内核 grep 2564 无硬编码（src/ include/ 零命中）②side_effect 表无行、
//   effect_info 2564 行即模板 ③registry moves 1520 无 2564，moves_lib/soul_lib
//   全 worktree 并集无 SKILL_EFFECT(2564)/SOUL_PROGRAM(2564)/kMark2564——真缺口。
//   ⚠️ soul 空间 2456 = 梅赫维特 4853 魂印（soul_lib/k421_meiheweite_lyrics.cpp），
//   同号不同义不涉本单。
// 先例：1285（pp_reduce 全槽，j_notdefeat_pink.cpp:107-108）、
//   900001（j_threshold_armor_random_pink.cpp:177-195，SKILL_INVALID 趟同原语）、
//   1450/1632（吸取=按实际扣回吸的体力版，j_pp_gate_drain.cpp:29-35）。
// 场景回归：test/scenario/scenario_8400_k453.cpp（8400 号段）。

#include "common.h"

namespace {

// ── 2564 —— 吸取对手所有技能{0}点PP值（纯扣除，无去向）──
// int_args = [owner, target, {0}=点数]（args_num=1）。
EffectResult effect_2564_pp_drain_all(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2564, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2564, args,
                     "2564 布局=[owner,target,点数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int drain = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     2564, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(drain > 0, 2564, args, "需要 点数 > 0");
    if (g_core && g_core->pp_reduce) {
        (void)g_core->pp_reduce(ctx, target, drain);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2564, effect_2564_pp_drain_all)
