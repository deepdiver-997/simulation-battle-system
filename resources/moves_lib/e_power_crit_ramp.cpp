#include <utils/sbs_rand.h>
// E8 · 递增/随机威力族批产（2026-09-29，工单：docs_local/docs/05-任务清单/E组批量效果工单.md E8）
//
// 家族成员（effect_info 模板 × args_num）：
//   604（4 参 [lo, hi, step, cap]）"威力随机，随机范围{0}-{1}，连续使用每次威力提升{2}，
//       最高提升至{3}" —— 圣水灵光闪 16204 [150,300,30,300]（威力 0，全凭 604 提供威力）、
//       火之奇决 19829、安达里士 19948 等 39 技。
//   696（3 参 [pct, step, cap]）"暴击率提升{0}%，每次使用额外提升{1}%，最高概率{2}%"
//       —— 天王圣剑斩 16886 [20,10,100]、幻生幻灭冲 19247、宁屹浅沫光 19922 等 37 技。
//
// 动工三查（2026-09-29）：内核 skills.cpp/battle_primitives.cpp 无 604/696 硬编码；
// moves_lib 无既有 SKILL_EFFECT —— 真缺口。
//
// 落点：
// ★ 604 = `ws.skill_power_view[owner]` 变威力通道（SkillView 写入即打"变威力"标记，
//   伤害管线前以当前状态重算覆盖，battleWorkspace.h §SkillView）。SKILL_EFFECT 时点写
//   即惠及本次攻击（重算在时点桶之后）——与 795 递增增伤同刻。威力值 = min(rand[lo,hi]
//   + step×(uses-1), cap)。uses 计数走 795/2396 同款 plugin_storage 每用 +1（"连续使用"
//   不做中断清零，与 795 家族口径一致，记档）；rand 消耗一次（威力域随机，非概率事件，
//   不走概率闸门，与伤害浮动同性质）。
// ★ 696 = 暴击率加算通道 `ws.crit_rate_add`（百分点；"rate = base×mod + add，add>0 即
//   引爆"，battleWorkspace.h）。★首用偏差★：暴击掷点在 query_usage ①.5，**先于**
//   SKILL_EFFECT 效果链——本次使用的 +{0}% 落不到本次掷点（1782 有盾先制/1002 条件先制
//   同款引擎约束，记档）。因此成长值必须跨回合持久：首次使用时挂常驻 ROUND_START 重写器
//   （TEAM 域 + on-stage 门，duration=-1 不可断），每回合写 min(pct+step×(uses-1), cap)
//   到 crit_rate_add（ws 每回合 reset → 重写器每回合补，58 圣光气同套路）。uses 计数
//   plugin_storage 每用 +1；"最高概率{2}%"封顶加算份额（rate≥100 引擎侧天然必暴）。
#include "common.h"

#include <memory>
#include <vector>

namespace {

constexpr int k604UsesKeyBase = 94060400;   // plugin_storage：604 每用计数（按 side 偏移）
constexpr int k696UsesKeyBase = 94069600;   // plugin_storage：696 每用计数（按 side 偏移）
constexpr int k696ArmedKeyBase = 94069650;  // plugin_storage：696 重写器幂等键（按 side 偏移）

int counter_next(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    const int uses = (it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second)) + 1;
    ctx->plugin_storage[key] = uses;
    return uses;
}

// ── 604 —— 威力随机 + 连用递增 + 封顶（变威力通道）────────────────────────────
EffectResult effect_604_random_power_ramp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 604, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 6, 604, args, "需要 [owner,target,lo,hi,step,cap]");
    const int owner = args.int_args[0];
    const int lo = args.int_args[2];
    const int hi = args.int_args[3];
    const int step = args.int_args[4];
    const int cap = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 604, args, "owner 越界");
    EFFECT_ARG_GUARD(lo >= 0 && hi >= lo && step >= 0 && cap >= lo, 604, args,
                     "参数域非法（要求 0<=lo<=hi、step>=0、cap>=lo）");
    const int uses = counter_next(ctx, k604UsesKeyBase + owner);
    const int ramp = step * (uses - 1);
    int power = (lo + (hi > lo ? ctx->rng().next() % (hi - lo + 1) : 0)) + ramp;
    if (power > cap) {
        power = cap;   // "最高提升至{3}"——封顶随机+递增的总威力
    }
    ctx->ws.skill_power_view[owner] = power;   // 写入即触发变威力重算（本次攻击生效）
    return EffectResult::kOk;
}

// ── 696 的常驻重写器：每回合把成长值写回 crit_rate_add ────────────────────────
EffectResult effect_696_crit_ramp_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int slot = args.int_args[1];
    const int pct = args.int_args[2];
    const int step = args.int_args[3];
    const int cap = args.int_args[4];
    if (owner < 0 || owner > 1 || slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    if (ctx->on_stage[owner] != slot) {
        return EffectResult::kOk;   // 携带者不在场不写（crit_rate_add 是 side 级槽，别惠及队友）
    }
    const auto it = ctx->plugin_storage.find(k696UsesKeyBase + owner);
    const int uses = it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second);
    if (uses <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.crit_rate_add[owner] +=
        static_cast<float>(std::min(pct + step * (uses - 1), cap));
    return EffectResult::kOk;
}

// ── 696 —— 暴击率成长（每次使用递增，跨回合持久）──────────────────────────────
EffectResult effect_696_crit_rate_ramp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 696, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 5, 696, args, "需要 [owner,target,pct,step,cap]");
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    const int step = args.int_args[3];
    const int cap = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 696, args, "owner 越界");
    EFFECT_ARG_GUARD(pct >= 0 && step >= 0 && cap >= 0, 696, args, "参数域非法");
    (void)counter_next(ctx, k696UsesKeyBase + owner);   // 每用 +1（重写器现读现写）
    // 常驻重写器（每侧只挂一次；携带者槽位捕获，on-stage 门在 tick 内判）。
    const int armed_key = k696ArmedKeyBase + owner;
    if (ctx->plugin_storage.count(armed_key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[armed_key] = 1;
    int slot = -1;
    for (int i = 0; i < 6; ++i) {
        for (const Skills& s : ctx->seerRobot[owner].elfPets[i].skills) {
            if (s.rawEffectRecords.end()
                != std::find_if(s.rawEffectRecords.begin(), s.rawEffectRecords.end(),
                                [](const official_data::SkillEffectRecord& r) {
                                    return r.effect_id == 696;
                                })) {
                slot = i;
                break;
            }
        }
        if (slot >= 0) {
            break;
        }
    }
    if (slot < 0) {
        return EffectResult::kOk;
    }
    Effect e(696, 0, owner, -1, EffectArgs(std::vector<int>{owner, slot, pct, step, cap}),
             &effect_696_crit_ramp_tick);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, owner,
                                                 /*duration=*/-1, ctx->roundCount);
    ce->source_id_ = 696;
    ce->scope_ = EffectScope::TEAM;   // 活过换宠；duration<0 不可被断回合
    ctx->register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(604, effect_604_random_power_ramp);
SKILL_EFFECT(696, effect_696_crit_rate_ramp);
