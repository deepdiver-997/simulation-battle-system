// J318 · 消除成功条件族 —— effect 655（1 宏 / 8 载体，2026-11-02，a01-jnight4）
//
// 家族语义：effect_info 模板「消除{0}状态，消除成功则{1}的{2}」（args_num=8）是
// **有损描述**——{0}/{1}/{2} 只露了前三位，实参结构由 8/8 载体切片 × 4399 图鉴文本
// 逐条反推钉死（多数读法，全部自洽）：
//
//   args[0] 消除对象：0 = 消除对手能力提升（clear_stat_boosts——查免消除强化
//                     STAT_CLEAR，被免返回 0 =「消除未成功」）；
//                     1 = 解除自身能力下降（clear_stat_drops——**不查任何免疫**
//                     （清弱化对目标有利，core_api 口径），返回清掉的槽数）。
//   args[1] 后缀目标：0 = 对手、1 = 自身。
//   args[2..7] 六槽增量，**官方语义序**（攻,防,特攻,特防,速,命中）→ 引擎等级槽
//              {0,2,1,3,4,5}（538 kSemanticToEngineSlot / batch_a kOfficialStatToSlot
//              同款映射）。⚠️ 属性排列双序坑（速查 §三）：16399「双攻-1」切片
//              [-1,0,-1,0,0,0] 落引擎槽 0(攻)/1(特攻)——按引擎序直读会错落
//              1(特攻)/2(防)，场景 T7620 以防槽 ==0 反证钉死。
//   增量按符号路由原语：负 → stat_drop（弱化原语：查 ATTACH_BAN/免弱、钳 -6，
//              actor 如实声明 = owner）；正 → stat_change（增益不查免弱）。
//              8/8 载体符合「负降对手 / 正升自身」惯例（16399/17018/17101 负→对手，
//              16426/17153/19615/19785/31450 正→自身），泛化按参数驱动、不逐载体特判。
//
//   「消除成功」门槛 = clear 返回值 > 0（目标状态存在判定 → 条件分支，K396 补偿
//   节点同构）：消强门 = 538/794/868/1010 同款（h_purge_success.cpp），
//   解除门 = J10 1115 同款（j_dispel_invert_success.cpp——清弱化返回 0 = 本无下降 =
//   「解除未成功」）。门槛失败 → 后缀六槽全不接。
//
// ⚠️ 同号不同物：soul 空间 effect 655 是 K312/K335 磨血族（SOUL_PROGRAM(655)，
//    resources/soul_lib/）——与本文件 SKILL_EFFECT(655)（moves 空间）分属两本注册账，
//    互不相干（看板行「moves 空间效果宏」警示即此）。
//
// 动工前置三查（2026-11-02）：①内核 grep \b655\b 零命中（无行为硬编码）；
//    ②custom_effect_programs 无 655 行（无数据程序短路）；③moves_lib 全库无
//    SKILL_EFFECT(655) 注册（batch_a/b 命中的全是 1655）——真缺口。
//
// ⛔ 同批 1296「{0}回合内受到攻击{1}%进入自然庇佑」（9 载体：26742 林海之息 /
//    28458 碧叶护花 / 28982 锋锐坚塔 / 31625 大自然庇佑 / 31893 暗香疏影阵 /
//    32662 逆界灵光 / 34632 悠扬入梦曲 / 34959 花叶纷飞 / 37119 根系）**记档停**：
//    目标状态「自然庇佑」未建模——registry.json anomalies 全 44 条无此状态、
//    DB battle_effects 全表 name/raw_json 0 命中（2026-11-02 复核，与
//    batch_c_skills.cpp 跳过清单 2026-09-27 同判）。apply_anomaly 原语无法施加
//    未建模状态、语义不明 → 按看板行口径记档停，待异常系统补录后按 406/190
//    「受到攻击窗口」同款落法补。
//
// 载体清单（side_effect.arg_count=8 切片，运行时切片口径 = side_effect 表优先，
// 19615/31450 链上兄弟效果各按自身 arg_count 续切——速查 §五双表口径）：
//   16399 合金烈爪 [1,0,-1,0,-1,0,0,0]  解除自身弱化，成功双攻-1（对手）
//   16426 钢翼切割 [1,1,0,0,0,1,0,0]    解除自身弱化，成功自身特防+1
//   17018 风凛一击 [1,0,0,0,0,0,-2,0]   解除自身下降，成功对手速-2
//   17101 毁灭进攻 [0,0,0,0,0,0,-2,-2]  消除对手强化，成功对手速-2命中-2
//   17153 觉醒粒子 [0,1,0,0,0,0,1,0]    消除对手提升，成功自身速+1
//   19615 狂野搏击 [0,1,0,1,0,1,0,0]+943[30]  消除对手提升，成功自身防/特防+1
//   19785 强力箭   [0,1,1,1,1,1,1,1]    消除对手提升，成功自身全属性+1
//   31450 碧水弦音 [0,1,0,0,1,0,1,0]+497[100,50,200]  消除对手提升，成功自身特攻/速+1
//
// 场景回归：test/scenario/scenario_7620_dispel_success_cond.cpp（7620-7629 号段）。

#include "common.h"

namespace {

// 官方语义序（攻,防,特攻,特防,速,命中）→ 引擎等级槽（0攻 1特攻 2防 3特防 4速 5命中）。
// 538 kSemanticToEngineSlot / 595 kOfficialStatToSlot 同款（1/2 两位互换）。
constexpr int kSemanticToEngineSlot[6] = {0, 2, 1, 3, 4, 5};

// ── 655 —— 消除{0}状态，消除成功则{1}的{2}（三参模板有损，八参实解见文件头）──
// int_args = [owner, target, 消除对象, 后缀目标, 攻,防,特攻,特防,速,命中]
//（build_effect_args_for_skill 固定前置 [owner,target]，六槽从 [4] 起，args_num=8
//  → int_count>=10）。
EffectResult effect_655_dispel_success_cond(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 655, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 10, 655, args,
                     "655 布局=[owner,target,消除对象,后缀目标,六槽×6]（int_count>=10）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int dispel_sel = args.int_args[2];
    const int suffix_target_sel = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 655,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(dispel_sel == 0 || dispel_sel == 1, 655, args, "需要 消除对象 ∈ {0,1}");
    EFFECT_ARG_GUARD(suffix_target_sel == 0 || suffix_target_sel == 1, 655, args,
                     "需要 后缀目标 ∈ {0,1}");
    if (!g_core || !g_core->stat_drop || !g_core->stat_change) {
        return EffectResult::kOk;
    }

    // 「消除成功」门槛：消除对象侧状态存在判定（clear 返回清掉的槽数，>0 即成功）
    int cleared = 0;
    if (dispel_sel == 0) {
        if (!g_core->clear_stat_boosts) {
            return EffectResult::kOk;
        }
        cleared = g_core->clear_stat_boosts(ctx, opp);   // 查免消除强化 STAT_CLEAR
    } else {
        if (!g_core->clear_stat_drops) {
            return EffectResult::kOk;
        }
        cleared = g_core->clear_stat_drops(ctx, owner);  // 不查免疫（对目标有利）
    }
    if (cleared <= 0) {
        return EffectResult::kOk;   // 消除未成功 → 后缀不接（K396 条件分支同构）
    }

    const int target = (suffix_target_sel == 1) ? owner : opp;
    for (int i = 0; i < 6; ++i) {
        const int delta = args.int_args[4 + i];
        if (delta == 0) {
            continue;
        }
        // 负 delta = 弱化 → stat_drop 的 amount 是降幅 magnitude（正数，595/477 同款换算，
        // 吃免弱）；正 delta = 提升 → stat_change（增益不查免弱）。
        if (delta < 0) {
            (void)g_core->stat_drop(ctx, target, kSemanticToEngineSlot[i], -delta, owner);
        } else {
            (void)g_core->stat_change(ctx, target, kSemanticToEngineSlot[i], delta);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(655, effect_655_dispel_success_cond);
