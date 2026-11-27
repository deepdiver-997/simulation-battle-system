#include <utils/sbs_rand.h>
// J 组 J159 · 变威力族三 —— 468×11 / 118×10 / 51×11（2026-10-04，worktree ../agents/j159）
//
// 家族语义（effect_info 官方模板即权威）：
//   468 [0] 回合开始时，若自身处于能力下降状态，则威力翻倍，同时解除能力下降状态
//   118 [2] 随机改为{0}~{1}点威力
//   51  [1] {0}回合自身的攻击力和对手相同
//
// 实现口径：
//   - 468 = 常驻 ROUND_START tick（696 重写器形：duration=-1 + TEAM + 携带者槽位
//     on-stage 门 + storage 旗防重复武装）：自身任一负槽 → 置旗 + clear_stat_drops
//     （CoreApi 142）。SKILL_EFFECT（97 通道）：旗立 → skill_power_view×2（不消费——
//     下回合 tick 重评；威力翻倍而攻击仍降 → 物理载体恰与降槽抵消，场景用此配平断言）。
//     ⚠️ 武装当回合（R1）tick 未跑 → 首次使用不吃翻倍=语义（"回合开始时"时点）。
//   - 118 = SKILL_EFFECT 绝对写 skill_power_view = rand[{0},{1}]（604 同款"威力域随机"
//     消耗一次 rand，非概率事件；"改为"=绝对置）。⚠️ batch_b 旧记档"118 运行时无参"
//     已过时——118 在 R1 切片修正清单（apply_all），side_effect.arg_count=2 已修。
//   - 51 = SKILL_EFFECT 立即拷 battle_attrs[自身][攻击]=battle_attrs[对手][攻击]
//     （武装回合即生效）+ InRounds({0}) ROUND_START tick 逐回合续拷（每回合
//     sync_workspace_from_on_stage 重灌 battle_attrs → 拷贝自愈，窗口过期自然还原）。
//     记档：只拷物攻字面（row 口径）；武装载体为属性技（无伤害面），实际生效自 R2 起。
//
// 载体切片（unity 库 json_each 精确匹配 11+10+11=32；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-04）：
//   468 []（零参）纯链×9（烈焰狂火杀 14051 物 pri0 等，16334 pri=2 弃用场景）
//   118 [lo,hi]：{150,220}×9/{130,190}——纯链×3（白羽净化 12078 [150,220] 物 pri0）
//   51  [回合]：{5}×4/{4}/{3}/… 纯链×2（变化术 20096 / 黑暗操控 20767 均 [5] cat4 pri0）
//
// 动工前置三查（2026-10-04 本 worktree 复核，含 CARRY_EFFECT）：①内核 grep 3 id
// 无硬编码 case ②effect_meta 无行 ③moves_lib 并集无注册——真缺口。
// 先例：696（常驻 ROUND_START 重写器+on-stage 门，e_power_crit_ramp）、604（rand 威力
//   绝对写，同文件）、97（skill_power_view×2 通道）、clear_stat_drops（CoreApi 142）。
// 场景回归：test/scenario/scenario_2500_power_view3.cpp（2500 号段，一文件三宏）。

#include "common.h"

#include <memory>

namespace {

constexpr int k468FlagBase = 990468000;    // +owner：回合开始"有降槽"旗
constexpr int k468ArmKeyBase = 990468010;  // +owner：tick 已武装旗

// ── 468 tick：回合开始判降槽 → 置旗 + 解除 ──
EffectResult j159_468_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->clear_stat_drops || !a.int_args || a.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = a.int_args[0];
    const int slot = a.int_args[1];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (c->on_stage[owner] != slot) {
        return EffectResult::kOk;   // 携带者不在场不判（旗是 side 级）
    }
    bool dropped = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (c->ability_levels[owner][i] < 0) {
            dropped = true;
            break;
        }
    }
    c->plugin_storage[k468FlagBase + owner] = dropped ? 1 : 0;
    if (dropped) {
        (void)g_core->clear_stat_drops(c, owner);   // "同时解除能力下降状态"
    }
    return EffectResult::kOk;
}

// ── 468 —— 回合开始时若有降槽：威力翻倍 + 解除（零参）──
EffectResult effect_468_drop_power_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 468, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 468, args,
                     "468 布局=[owner,target]（on_selected 绑定）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 468, args, "需要 owner ∈ {0,1}");
    // 常驻 tick（每侧只挂一次；696 同款 slot 扫描 + on-stage 门）。
    const int arm_key = k468ArmKeyBase + owner;
    if (ctx->plugin_storage.count(arm_key) == 0) {
        int slot = -1;
        for (int i = 0; i < 6 && slot < 0; ++i) {
            for (const Skills& s : ctx->seerRobot[owner].elfPets[i].skills) {
                if (std::find_if(s.rawEffectRecords.begin(), s.rawEffectRecords.end(),
                                 [](const official_data::SkillEffectRecord& r) {
                                     return r.effect_id == 468;
                                 }) != s.rawEffectRecords.end()) {
                    slot = i;
                    break;
                }
            }
        }
        if (slot >= 0) {
            ctx->plugin_storage[arm_key] = 1;
            Effect e(468, 0, owner, -1, EffectArgs(std::vector<int>{owner, slot}),
                     &j159_468_tick);
            auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, owner,
                                                         /*duration=*/-1, ctx->roundCount);
            ce->source_id_ = 468;
            ce->scope_ = EffectScope::TEAM;   // 活过换宠；duration<0 不可被断回合
            ctx->register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
        }
    }
    // 本次使用：回合开始旗立 → 威力翻倍（97 通道；旗由下回合 tick 重评，不消费）。
    const auto it = ctx->plugin_storage.find(k468FlagBase + owner);
    if (it != ctx->plugin_storage.end() && any_int_or(it->second) == 1) {
        const int view = ctx->ws.skill_power_view[owner];
        if (view > 0) {
            ctx->ws.skill_power_view[owner] = view * 2;
        }
    }
    return EffectResult::kOk;
}

// ── 118 —— 随机改为{0}~{1}点威力 ──
// int_args = [owner, target, lo, hi]（args_num=2）。
EffectResult effect_118_random_power(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 118, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 118, args,
                     "118 布局=[owner,target,下限,上限]");
    const int owner = args.int_args[0];
    const int lo = args.int_args[2];
    const int hi = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 118, args, "需要 owner ∈ {0,1}");
    // 下限允许 0：混色 38237 [0,255]（fuzz seed 267758 实测——0~255 即设计本意）
    EFFECT_ARG_GUARD(lo >= 0 && hi >= lo && hi > 0, 118, args, "需要 0 ≤ 下限 ≤ 上限 且 上限 > 0");
    ctx->ws.skill_power_view[owner] = choose_skill_range(*ctx, 118, 0, owner, args.int_args[1], lo, hi);   // "改为"=绝对置（604 同款）
    return EffectResult::kOk;
}

// ── 51 tick：攻击力续拷（每回合 sync 重灌 → 拷贝自愈）──
EffectResult j159_51_copy_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    c->ws.battle_attrs[self][NumericalPropertyIndex::PHYSICAL_ATTACK] =
        c->ws.battle_attrs[1 - self][NumericalPropertyIndex::PHYSICAL_ATTACK];
    return EffectResult::kOk;
}

// ── 51 —— {0}回合自身的攻击力和对手相同 ──
// int_args = [owner, target, rounds]（args_num=1）。
EffectResult effect_51_copy_attack(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 51, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 51, args,
                     "51 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     51, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 51, args, "需要 回合 > 0");
    // 武装回合立即生效一次（本轮攻击即同攻）；后续回合由 InRounds ROUND_START tick 续拷。
    ctx->ws.battle_attrs[self][NumericalPropertyIndex::PHYSICAL_ATTACK] =
        ctx->ws.battle_attrs[opp][NumericalPropertyIndex::PHYSICAL_ATTACK];
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 51, rounds, {self},
                          &j159_51_copy_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(468, effect_468_drop_power_double)
SKILL_EFFECT(118, effect_118_random_power)
SKILL_EFFECT(51, effect_51_copy_attack)
