// K269 概率令对手害怕族 —— effect 186（1 宏 / 1 行）
//（艾辛格 2001；号段 4250-4259，worktree ../agents/k269，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 186，2026-10-21）：
//   row184 2001 args[10,0]（kind[10]）「10%令对手害怕」
//   → db[0]=10 = 概率%（与 tips 互锚）；db[1]=0 废位不消费（K1 纪律——tips 定死害怕，
//     不照抄 K2 306 的 db[1]=异常码 形：本行 db[1]=0 ≠ Fear(6)，照抄会落麻痹）；
//     害怕 = AbnormalStatusId::Fear（tips 单行定值）。
// ⚠️ effect_info 186 同号是技能空间模板（"后出手时…提升等级"，不同文无关）；4399 无
//    此宠魂印文本（soul_seal=none）——tips 为唯一权威（SOP §六.1）。
// ⚠️ 锚点记档：光杆句无时点锚，按"攻击附带"定例落地——EVENT_SKILL_USED own + 攻击技
//    （!used_attribute，miss 也算"攻击"，E13 使用即触发；K164 116 / K2 265·358 同窗）。
//    属性技不触发（"攻击"判据，场景 T4253 钉）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b186\b 无硬编码；②effect_info 186
//    技能空间模板与魂印无关；③soul_lib/registry 无 186 注册——真缺口。
// 实现：apply_anomaly_checked 申报制（概率门在原语内单掷，Modern 全链吃抗性/弹控/亮节
//   票，K2 口径；pct<=0 本地短路 = 场景关闸档）；arm 挂 OPERATION_ENTER_EXIT_STAGE
//   （early；plugin_storage 旗标防再登场双挂）。
// 注册：SOUL_PROGRAM(186) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4250_prob_fear.cpp（号段 4250-4259）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 186;
constexpr int kWiredKeyBase = 990769000;   // +owner：watcher 已挂旗标（防再登场双挂）

// 概率：宿主行 db[0]；行缺失按 tips 兜底 10；<=0 原样返回（调用方短路 = 关闸档）。
int k269_fear_pct(BattleContext* ctx, int owner) {
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return 0;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (!db.empty()) {
        return db[0];
    }
    return 10;
}

// arm：EVENT_SKILL_USED（own+攻击技）→ db[0]% 令对手害怕（Fear，申报制单掷）。
EffectResult k269_186_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 攻击技才触发（光杆句按"攻击附带"定例，属性技不算）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const int pct = k269_fear_pct(c, owner);
            if (pct <= 0) {
                return;   // 0% 关闸（与 K2 apply_chance_anomaly 同门，不耗 rand）
            }
            (void)g_core->apply_anomaly_checked(c, 1 - owner,
                                                static_cast<int>(AbnormalStatusId::Fear),
                                                random_anomaly_duration(), owner, pct,
                                                ChanceSource::SoulMark);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（186 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k269_186_arm,
                        /*once=*/false, /*early=*/true)});
