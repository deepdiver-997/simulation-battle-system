// H 组 H1 · 消强成功族 —— 538×23 / 548×22（2026-09-30，worktree ../agents/h1）
//
// 家族语义（effect_info 模板即权威）：
//   538（args_num=6）"消除对手能力提升状态，若消除成功则自身{0}"——{0} = 六元掩码
//     （每槽 0..6 级），**官方语义序**（攻,防,特攻,特防,速,命中）→ 引擎等级槽
//     {0,2,1,3,4,5}（477/745 同款 kSemanticToEngineSlot，防/特攻读反即错）。
//   548（args_num=1）"消除对手能力提升状态，消除成功后恢复{0}点体力"——定点恢复，
//     走 heal_amount（吃封回血 + 恢复效果修正，官方回血统一出口）。
//
// 4399 双源交叉验证（reference/scrape_4399/out/pets_full.jsonl，切片合法载体逐条核）：
//   - 锁魂镰刀 15075 [0,0,0,0,1,1]="命中+1 速度+1"（槽4=速/槽5=命中）
//     雷暴火环 15564 [0,0,0,0,0,1]="命中+1"；划空神光 16862 [1,0,0,0,0,0]="攻击+1"
//     绝尘剑 16881 [2,0,0,0,0,0]="攻击＋2"；吸收脉冲 17517 [1,0,0,0,1,0]="攻击+1、速度+1"
//     封灭决 19209 [1,1,1,1,1,1]="全属性+1"；强劲风力 31070 [0,0,2,0,2,0]="特攻+2，速度+2"
//     蓄势地裂 34885 [2,0,0,0,2,0]="速度+2、攻击+2"；不灭钳杀 34311 [0,2,0,2,0,0]="防御+2、特防+2"
//     喔喔突袭 34181 [0,0,0,0,1,1]="速度+1、命中+1"
//     落绒闪影 37698 [2,1,0,1,0,0,…]="攻击+2，防御+1，特防+1"（语义序 idx1=防/idx3=特防 钉死）
//     → 语义序全部吻合（若按引擎序直读，强劲风力会变成"防+2速+2"，与文本矛盾）。
//   - 548 幻灭之戟 15289 [200]="恢复200"、斩灵涡轮 16208 [250]="恢复250"、
//     重影弥生 34254 [200,…]="恢复200点体力"（首参）、念愈术 38214 [300,…]="恢复300"（首参，
//     200=链上护盾）、文本校对 38333 [400,…]="恢复400"、巨岩裂变 34748 [300,…]="恢复300"、
//     弱点暗杀 34268 [250,…]="恢复250"、狂岩碎心斩 31995 [200,30]="恢复200"。
//
// ⚠️ 切片双表口径病载体记档（816 修复前不信任切片，修复归 R 组；本文件实现参数驱动、
//    不逐载体特判，场景只用双源一致载体）：
//   - 538：29398 彼空都·蜃气楼 [2,1,50]（仅 3 位不足六元掩码）；33903 魂·诸法尘影
//     [2,40,20,100]（4399 原文 538 位语义="对手下回合先制-2"，非"自身提升"——载体特例）；
//     35035 风过叶飘零 [1,300,3]（原文无消强提升段）；36471 镜·灼渊龙息 [1,2,100,…]
//     与 37964 魂·岳鉴诸天 [2,200,…]（100/200 非法等级值）；37524/37628/37630 死亡之镰系
//     [1,4,30,80,23]（4399 无消强语义）；37118 分支 [0]（占位技无消强）。
//   - 548：36485 夜灵闪光 [50]（4399 原文="下回合受伤降50%"，无消强恢复段）；33983
//     轮回证道破 [50,50]（原文 548 位="造成伤害的50%恢复"，吸血语义非定点恢复）；
//     16697 乘风追命斩 [3,1,300]（4399="恢复300"——**548 参数在切片末位**，切片排列
//     逐载体不一，佐证"不信任切片"纪律；运行时切参归 R 组）。
//   - 4399 缺文但切片合法（前六位均 0..6）按切片 + 模板权威：18069 霹雳雷火剑
//     [1,1,1,1,1,1,…]、30688 清羽幻云决 [2,0,0,0,2,0,…]、32086 幻舞迷光阵 [0,0,0,0,1,1,…]、
//     34672 枫落不知秋 [2,0,0,0,1,0,…]、34963 怨灵索命 [2,0,0,0,1,0,…]、35153 灰烬幻象
//     [0,2,0,2,0,0]、35159 腐朽纹路 [2,0,0,0,2,0,…]、35248 迎风雪舞 [2,0,0,0,2,0,…]、
//     38557 翼·清风飏云 [2,2,3,0,0,0,…]；30400 火刃旋风前 2 位 [40,30] 是链上概率效果
//     （4399"40%害怕…额外附加30%"），538 六元 = 第 3..8 位 [1,1,1,1,1,1]="全属性+1"。
//
// 动工前置三查（2026-09-30）：①内核 grep 无 538/548 硬编码 ②custom_effect_seed.sql
// 无这两 id 行（无窗口家族声明——两者均 SKILL_EFFECT 即时型）③moves_lib 无既有注册
// （SKILL_EFFECT(538/548) 零命中）——真缺口。消强门槛先例 = 794（clear_stat_boosts>0，
// 868/1010 同款）；语义序映射先例 = 745 kSemanticToEngineSlot。
//
// 时序：两效果都是"消强→门槛→自身收益"即时链，SKILL_EFFECT 桶内先 clear 后
// stat_change/heal_amount（官方文本语序）。stat_change 查 STAT_BOOST 封锁
// （"无法处于能力提升状态"→ BLOCKED 该槽不动，引擎统一行为，多槽部分成功记档）。
//
// 场景回归：test/scenario/scenario_790_purge_success.cpp（790-799 号段，一文件覆盖
// 两效果五载体；538 用绝尘剑/不灭钳杀/封灭决钉攻槽与防特防互换、548 用幻灭之戟/
// 斩灵涡轮钉定点恢复与封回血）。

#include "common.h"

namespace {

// 官方语义序（攻,防,特攻,特防,速,命中）→ 引擎等级槽（0攻 1特攻 2防 3特防 4速 5命中）。
// 745 kSemanticToEngineSlot / stat_dispel.cpp kStatMap 同款（1/2 两位互换）。
constexpr int kSemanticToEngineSlot[6] = {0, 2, 1, 3, 4, 5};

// ── 538 —— 消除对手能力提升状态，若消除成功则自身{0}（六元掩码·语义序）──
// int_args = [owner, target, 攻,防,特攻,特防,速,命中]（build_effect_args_for_skill 固定
// 前置 [owner,target]；掩码从 [2] 起，args_num=6 → int_count>=8）。
EffectResult effect_538_purge_then_boost(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 538, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 8, 538, args,
                     "538 布局=[owner,target,攻,防,特攻,特防,速,命中]（int_count>=8）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 538,
                     args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->clear_stat_boosts || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts(ctx, opp) <= 0) {
        return EffectResult::kOk;   // "消除成功"门槛（消强族同款，794/868/1010 先例）
    }
    for (int i = 0; i < 6; ++i) {
        const int levels = args.int_args[2 + i];
        if (levels > 0) {
            (void)g_core->stat_change(ctx, owner, kSemanticToEngineSlot[i], levels);
        }
    }
    return EffectResult::kOk;
}

// ── 548 —— 消除对手能力提升状态，消除成功后恢复{0}点体力 ──
// int_args = [owner, target, {0}=恢复点数]（args_num=1 → int_count>=3）。
EffectResult effect_548_purge_then_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 548, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 548, args,
                     "548 布局=[owner,target,恢复点数]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 548,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 548, args, "需要 恢复点数 > 0");
    if (!g_core || !g_core->clear_stat_boosts || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts(ctx, opp) <= 0) {
        return EffectResult::kOk;   // "消除成功"门槛：失败不恢复
    }
    (void)g_core->heal_amount(ctx, owner, amount);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(538, effect_538_purge_then_boost);
SKILL_EFFECT(548, effect_548_purge_then_heal);
