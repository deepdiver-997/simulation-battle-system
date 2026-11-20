// H 组 H8 · 恢复/吸取窗口族 —— 509×27（532/597/707 已有实现，见下）（2026-09-30，worktree ../agents/h8）
//
// ⚠️ 工单四 id 的实现现状盘点（动工三查③修正工单预期）：
//   - 532（round_window.cpp:496 heal_equal_percent）/ 597（:497 round_drain）/
//     707（:498 round_heal_double）**已实现且有注册**——本任务不重做、不重复注册；
//     57 直系窗口范式（ROUND_START tick + 使用技能判定）同文件 485 行起。
//   - 真缺口 = **509**（全库 moves_lib/soul_lib 零注册；mistike_skills.cpp 的 "509"
//     是行号巧合非效果 id）。
//
// 509（args_num=1）"恢复自身已损失体力值的1/{0}"（effect_info 模板即权威）：
//   - 4399 双源：叶语风花闪 15326 [2]="恢复当前受损体力值1/2的体力"、沼泽狂风斩
//     16789 [3]="恢复当前受损体力值1/3的体力"、灭世斩杀 16877 [2]/圣光大师 26160 [2]
//     同文（"已损失体力值"="当前受损体力值"，双表述同义）。
//   - 分子分母互换 = 43 直系（"恢复自身最大体力的1/{0}"，43 的分子=最大体力；509
//     的分子=**已损失**体力 = max_hp − hp）。落点 heal_amount（按具体数值恢复，
//     吃封回血 + 恢复效果修正 + 记 last_heal——回血统一出口，509 的定点数值语义）。
//     折算整型除法（已损失×1/{0} 向下取整，引擎固有一贯口径）；满血（已损失 0）
//     走 heal_amount 的 amount<=0 出口（last_heal=0）。
//   - 切片口径：单链载体 [2]/[3] 干净（15326/16789/16877/26160 等逐核）；链式
//     （17393 [509,508]、17531 [454,509]、18118 [180,509]、22497/23154/24155）按
//     各自 arg_count 切、509 段单参合法——邻接效果 508/180 已实现，444/454 未逐一
//     核查，场景只测单链载体（双源已验）。
//
// 动工前置三查（2026-09-30）：①内核 grep 509 无硬编码 ②custom_effect_seed.sql
// 零命中 ③moves_lib 零注册（532/597/707 的既有注册见上——三查③的"已有实现"分支，
// 复用不重做）。
//
// 场景回归：test/scenario/scenario_860_lost_hp_heal.cpp（860-869 号段）：压血精确断
// （损失值×1/{0} 整除 → hp 终值精确等式）、满血对照（last_heal==0）、封回血（heal_amount
// 统一出口被 HEAL_BLOCK 拦）、双载体分母 2/3 各一。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// ── 509 —— 恢复自身已损失体力值的1/{0} ──
// int_args = [owner, target, {0}=分母]。
EffectResult effect_509_heal_lost_hp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 509, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 509, args,
                     "509 布局=[owner,target,分母]（int_count>=3）");
    const int owner = args.int_args[0];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 509, args, "owner 越界");
    EFFECT_ARG_GUARD(denom > 0, 509, args, "需要 分母 > 0");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    // 分子 = 已损失体力（max − current，43 的分子分母互换）；整型除法向下取整。
    const ElfPet& pet = ctx->getPet(owner);
    const int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    const int lost = max_hp - pet.hp;
    const int amount = lost / denom;
    (void)g_core->heal_amount(ctx, owner, amount);   // amount<=0（满血）由原语出口统一处理
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(509, effect_509_heal_lost_hp);
