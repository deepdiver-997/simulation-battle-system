// H 组 H3 · 连击/递增固伤族 —— 429×25 / 162×26（2026-09-30，worktree ../agents/h3）
//
// 家族语义（effect_info 模板即权威）：
//   429（args_num=3）"附加{0}点固定伤害，连续使用每次增加{1}点固定伤害，最高附加
//     {2}点固定伤害"——首用附 {0}，同一技能连用第 n 次附 min({0}+(n-1)×{1}, {2})。
//   162（args_num=1）"若对手处于异常状态，则附加{0}点伤害"——**任意**异常判据
//     （foe_has_any_anomaly 遍历官方异常码 0..43 查 has_active_anomaly，745/F3 1214
//     同款查询面；"指定异常"是 609 的语义，两者模板文本可区分）。
//
// 4399 双源验证（reference/scrape_4399/out/pets_full.jsonl）：
//   - 429 异次元星空 13535 [25,25,100]="附加25点伤害，连续使用每次增加25点伤害，
//     最高附加100点"；鬼神双牙斩 16271 [70,60,250]="附加70固伤，连续使用每次增加
//     60固伤，最高附加250固伤"；草叶突击斩 19379 [50,50,150] 同构——切片三参
//     [base,step,cap] 与文本逐字对应。
//   - 162 孢子碎击炮 12799 [50]="若对手处于异常状态，则附加50点伤害"；遮天光束斩
//     17024 [200]、空宇惊变 34094 [300]、龙枪合一破 16536 [100] 同构。
//
// 连用判定 = effect_9/1443 权威口径逐字照抄（damage_modifier.cpp / f_ramp_family.cpp）：
//   ctx->skill_use_seq[owner]（每方技能使用序号）+ plugin_storage 存 [上次序号, 技能id,
//   连用次数]——本次序号==上次+1 且同一技能 → 连用继续；序号跳变 → 断连重置（count=1，
//   金额=base）；嗑药/被控回合不出手序号不动 → 天然不断连（effect_9 已定口径）。
//   键段 990543290+owner*3（grep 全库无撞；1443 用 990544300 相邻不重叠）。
//
// 落点：SKILL_EFFECT 桶 deal_pink_damage(FIXED, amount)（G5 192 同桶——当次攻击效果
// 结算期、ATTACK_DAMAGE 红伤之前，粉在红前是本家族既有时序；FIXED 点数档吃护罩）。
//
// 动工前置三查（2026-09-30）：①内核 grep 429/162 仅命中"idx=429 时点表"官方文档
// 编号注释（battleFsm/battle_primitives/abnormal-types 三处，与效果 id 429 无关）
// ②custom_effect_seed.sql 零命中（即时型无窗口声明）③moves_lib 无既有注册
// （SKILL_EFFECT(429/162) 零命中；1626/1620~1629 是别的 id，LIKE 噪声）——真缺口。
//
// 切片口径：429 全部单链载体切片 [base,step,cap] 干净（13535/14809/15494/16200/16271/
// 17313/19379 七条逐核）；链式载体（16345 [429,148]、17323 [715,429]、17798 [429,180]、
// 18126 [695,429]、19330 [429,5,5]、30948/31035/32314/32960/34418/34993/36894/37493/
// 37705/38328）切片顺序跟链序、按各自 arg_count 切，429 段三参均合法——但邻接效果
// 是否已实现未逐一核查，场景只测单链载体（双源已验）。162 单链载体 24 条切片
// [50]~[300] 全干净（12799/16536/16653/16726/16837/17024/18318/34094/34543/34603/
// 34674/35570 逐核或有 4399）；LIKE '%162%' 命中的 1620~1629 为其他效果 id，已剔除。
//
// 场景回归：test/scenario/scenario_810_ramp_fixed.cpp（810-819 号段，一文件覆盖两
// 效果四载体；粉伤断言 = 总掉血 − raw 台账（红伤浮动被台账吸收）− 中毒 tick（确定性）。）

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k429KeyBase = 990543290;   // +owner*3：[+0]=上次序号 [+1]=技能id [+2]=连用次数

// ── 429 —— 附加{0}点固定伤害，连续使用每次增加{1}点，最高附加{2}点 ──
// int_args = [owner, target, base, step, cap]（模板参数切片从 [2] 起）。
EffectResult effect_429_combo_ramp_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 429, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 429, args,
                     "429 布局=[owner,target,base,step,cap]（int_count>=5）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int base = args.int_args[2];
    const int step = args.int_args[3];
    const int cap = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     429, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(base > 0 && step > 0 && cap > 0, 429, args, "需要 base/step/cap > 0");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    // 连用判定（effect_9/1443 权威口径）：skill_use_seq + plugin_storage 连用账本。
    const int slot = ctx->executing_skill_slot(owner);
    if (slot < 0 || slot >= 5) {
        return EffectResult::kOk;
    }
    const int skill_id = ctx->getPet(owner).skills[static_cast<std::size_t>(slot)].id;
    if (skill_id <= 0) {
        return EffectResult::kOk;
    }
    const int kLastKey = k429KeyBase + owner * 3;
    const int seq_now = ctx->skill_use_seq[owner];
    int last_seq = -1;
    int last_skill = -1;
    int count = 0;
    if (ctx->plugin_storage.count(kLastKey)) {
        last_seq = any_int_or(ctx->plugin_storage[kLastKey]);
        last_skill = any_int_or(ctx->plugin_storage[kLastKey + 1]);
        count = any_int_or(ctx->plugin_storage[kLastKey + 2]);
    }
    const bool consecutive =
        (last_seq >= 0 && seq_now == last_seq + 1 && last_skill == skill_id);
    count = consecutive ? count + 1 : 1;
    ctx->plugin_storage[kLastKey] = seq_now;
    ctx->plugin_storage[kLastKey + 1] = skill_id;
    ctx->plugin_storage[kLastKey + 2] = count;
    const int amount = std::min(base + (count - 1) * step, cap);   // 首用 = base
    (void)g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

// ── 162 —— 若对手处于异常状态，则附加{0}点伤害 ──
// int_args = [owner, target, {0}=附加点数]（args_num=1 → int_count>=3）。
EffectResult effect_162_anomaly_bonus_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 162, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 162, args,
                     "162 布局=[owner,target,附加点数]（int_count>=3）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     162, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 162, args, "需要 附加点数 > 0");
    if (!g_core || !g_core->deal_pink_damage || !g_core->has_active_anomaly) {
        return EffectResult::kOk;
    }
    bool foe_anomaly = false;   // 任意异常（745/F3 1214 同款遍历官方异常码）
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, target, id)) {
            foe_anomaly = true;
            break;
        }
    }
    if (!foe_anomaly) {
        return EffectResult::kOk;   // 对手无异常：不附加
    }
    (void)g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(429, effect_429_combo_ramp_fixed);
SKILL_EFFECT(162, effect_162_anomaly_bonus_fixed);
