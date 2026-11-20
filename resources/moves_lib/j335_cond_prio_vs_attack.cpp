// J335 · 当回合条件先制 —— 1774「若对手当回合使用的技能为攻击技能则自身额外
// 先制+1」（1 宏落地，K425 续单）
// （看板 J335，a01-j335 2026-11-07，worktree ~/.agent-workspaces/workspaces/sbs-local/j335，
//  分支 feat/j335；场景号段 8080-8089（K425 分配段续用，K425 未占用），场景
//  test/scenario/scenario_8080_cond_prio_vs_attack.cpp。DB 侧 = j4 扩行补丁
//  tools/db_patches/j4/selection_routes_expansion_5.sql（custom_skill_selection_routes
//  36→37 行，选择期路由 plain 适配）；本文件 = 路由行的插件消费端（clone_effect
//  命中本注册，缺注册时 loadSkills fail-closed 报「clone 无实现」——部署绑定
//  「新 DB + 本 dylib」，J327 同款原子发布门）。）
//
// 行清单（官方现查 effect_info.info 全文；args_num 与载体切片逐位=0；2026-11-07，
//   逐行核算见 k425_tiaojian_xianzhi_pool.cpp 头注行清单，本单抽验 4 条复算一致）：
//   1774 [0] "若对手当回合使用的技能为攻击技能则自身额外先制+1"（纯条件先制）
//        载体 8 条：28051 启明之辰[1774,191,847,1342]（4408 派利尔）、
//        28366 饭后甜点[1617,1774,870]（4526 华夫家族）、
//        28668 安魂引[1774,191,426,1400]（3941 霍德尔隐藏技）、
//        35652 几曾识干戈[697,1568,1774,1395]（4327 极战·阿尔斯兰）、
//        35786 兽灵震谷爪[1774,664,1736]（4353 花寅）、
//        35907 御风无痕[1774,1099,769]（4376 圣谕之风·岚）、
//        36277 雾停枝[1774]（4448 流荧·雪，纯单载体）、
//        36411 灰刻截光破[1774,925,937]（4472 夜罗·琉彩）；链上 16 兄弟效果全注册。
//
// 动工前置四查（2026-11-07，本 worktree + 主库现查；K425 头注四查同结论复验）：
//   ① 内核 grep 1774：src/ include/ 零命中（rc=1），零行为硬编码；
//   ② effect_info 在列（args_num=0=side_effect.arg_count）；8 载体切片自洽；
//     custom_effect_programs / custom_effect_overrides 零命中（0/0）；
//   ③ 全库并集无注册：registry moves=1456 无 1774；resources/ SKILL_EFFECT(1774 /
//     SOUL_PROGRAM(1774 零命中；boss_invalid_effects.json 命中 = 魂印空间
//     effect_icon 同号行（K425 已溯源），非撞号；
//   ④ 废弃 parser 覆盖面：模板零占位符、不含两刀四/五形态（K425 头注 ④ 同文）——
//     本单注册后 clone_effect 先命中（选择期路由优先于 parser 通道）。
//
// 口径记档：
//   - 门判据 = **对手本回合所选技能为攻击技**（选择期语义）：roundChoice[opp][0]==
//     SELECT_SKILL 且 skills[idx].type != SkillType::Attribute——2273 同形判据
//     （artemis_skills.cpp effect_2273_priority_vs_attack），走 common.h
//     selected_skill_is_attack 统一判据（2026-10-03 用户裁定：本助手就是
//     条件先制等 MOVE_RIGHT 判定的指定面；battleWorkspace.h 注释同口径）。
//     嗑药（USE_MEDICINE）/切宠/ NONE 等非选技动作 roundChoice[opp][0]!=1 → 门闭。
//   - 出 = ws.preemptive_level[owner] += 1（零参宏，量写死模板文案 +1，无行参可
//     消费——§七点五口径）。执行时点 = MOVE_RIGHT 桶（on_selected 统一注册），
//     先手比较前生效：先制等级不等时只看等级不看速度（结构断言依据，
//     battleFsm handle_BattleFirstMoveRight）。
//   - 条件逐回合重算（不缓存，6450/J331 同款口径）：节点在 FIRST_MOVE_RIGHT
//     桶内即判即用，无跨回合状态、不建功能槽。
//   - 全效果零 rand 消费（判据全为确定值）→ 场景无随机精确断言。
//
// 存储键段（新增前已 grep 全库无撞；tools/check_storage_keys.py 门禁兜底）：
//   - 审计槽 10<id>000x 段（J331/K423 同款）：
//     1017740001(+owner)=1774 授予的先制量（缺席=门闭）
//
// 先例：2273（对手选攻击技→先制+3，effect_register_state 旧形态存量，本单不翻案）、
//   1956/1292（选择期条件先制体 + 审计槽，j_cond_prio3.cpp）、
//   1940/1941/2133（选择期路由体 + 审计槽，j331_marker_cond_prio.cpp）。
// 场景回归：test/scenario/scenario_8080_cond_prio_vs_attack.cpp（号段 8080-8089）。

#include "common.h"

namespace {

// ── 审计槽 ──────────────────────────────────────────────────────────────
constexpr int k1774Audit = 1017740001;   // +owner：授予的先制量（缺席=门闭）

}  // namespace

// ── 1774 —— 若对手当回合使用的技能为攻击技能则自身额外先制+1（零参；选择期路由体）──
// int_args = [owner, target]（on_selected 绑定；零参效果无行参槽）。
EffectResult effect_1774_cond_prio_vs_attack(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1774, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1774, args,
                     "1774 布局=[owner,target]（on_selected 绑定）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1774, args, "需要 owner/target ∈ {0,1} 且互异");
    if (!selected_skill_is_attack(ctx, opp)) {
        return EffectResult::kOk;   // 门闭：对手未选攻击技（属性技/嗑药/切宠）→ 不加先制
    }
    ctx->ws.preemptive_level[self] += 1;   // 零参宏：量写死模板文案 +1（§七点五口径）
    ctx->plugin_storage[k1774Audit + self] = 1;
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(1774, effect_1774_cond_prio_vs_attack)
