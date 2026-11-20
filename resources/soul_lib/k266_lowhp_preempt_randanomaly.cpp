// K266 低体先制+2+随机异常族 —— effect 366（1 宏 / 1 行）
//（忍王·壬无月 2771；号段 4220-4229，worktree ../agents/k266，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 366，2026-10-21）：
//   row148 2771 args[1,15]（kind[9,10]）
//   「自身体力小于1/2时，所有攻击技能先制+2；每次直接攻击都有15%概率使对手进入随机异常状态」
//   → db[0]=1 语义不明不消费（K1/K2 纪律：358 [1,1,…] 行首同款）；db[1]=15 = ②句概率%
//   （与 tips 互锚）；低体 1/2 阈值与先制量 2 为单行定值，按 tips 落地。
//   ⚠️ 行参读取走宿主 owned_int_args（K236 同款）——bind_soulmark_args 丢行参前两位，
//   [1,15] 恰全在前两位 → 节点绑定行参恒空，类型化 a.row 读不到 15。
// ⚠️ effect_info 无 366 模板行（SOP §六.1：模板缺失按 tips 落地，K194 先例）。
//
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b366\b 无硬编码；②effect_info 366
//    无模板行；③soul_lib/registry 无 366 注册——真缺口。
// 实现（板行处方：FMR 桶写 preemptive_level + attach_random_anomalies）：
//   ① BATTLE_FIRST_MOVE_RIGHT 桶（STAGE）：onstage 且 hp*2 < max（"小于1/2"，×2 防
//   整除截断）→ ws.preemptive_level[owner] += 2。pl 的 memset 在 MOVE_RIGHT 时点
//   （SOP §二.2），FMR 桶写即当回合生效；条件无回合锚 → FMR 处直读当前血（连续条件
//   语义；648 的 ROUND_START 账本形态仅限"回合开始时"显式锚文，本行不适用）。
//   "攻击技能先制+2"按 K218 277「所有攻击先制+1」定例落地为回合级 pl 写（先制决议在
//   选技之前，引擎无逐技先制通道可表达"仅攻击技"，记档）。
//   ② EVENT_SKILL_USED watcher（K164 同窗：own + !used_attribute=攻击技，miss 也算
//   "直接攻击"，E13 使用即触发）→ attach_random_anomalies(对手侧, 全池, 1 种,
//   db[1]%, owner, SoulMark)——概率门在原语内单掷（禁本地 roll 双掷）；arm 挂
//   OPERATION_ENTER_EXIT_STAGE（early；plugin_storage 旗标防再登场双挂）。
// 注册：SOUL_PROGRAM(366) 双节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4220_lowhp_preempt_anomaly.cpp（号段 4220-4229）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 366;
constexpr int kWiredKeyBase = 990766100;   // +owner：watcher 已挂旗标（防再登场双挂）

// ②句概率：宿主行 db[1]（db[0]=1 不消费）；行缺失按 tips 兜底 15；db[1]<=0 原样透传
// （原语概率门 0/100 短路不耗 rand → 场景可作"关闸"对照档）。
int k266_anomaly_pct(BattleContext* ctx, int owner) {
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return 0;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 1) {
        return db[1];
    }
    return 15;
}

// ① FMR 桶：自身 hp < 1/2 → 当回合所有攻击先制 +2（memset 后写）。
EffectResult k266_366_move_right(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    if (self.hp > 0 && self.hp * 2 < resolve_pet_max_hp(self)) {
        ctx->ws.preemptive_level[owner] += 2;
    }
    return EffectResult::kOk;
}

// ② arm：EVENT_SKILL_USED（own+攻击技）→ db[1]% 让对手进随机异常（全池挑 1）。
EffectResult k266_366_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->attach_random_anomalies) {
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
            if (!c || !g_core || !g_core->attach_random_anomalies) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 攻击技才触发（used_attribute=声明类型，属性技非"直接攻击"）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            (void)g_core->attach_random_anomalies(c, 1 - owner, anomaly_pool_all(),
                                                  /*count=*/1,
                                                  k266_anomaly_pct(c, owner),
                                                  owner, ChanceSource::SoulMark);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（366 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k266_366_arm,
                        /*once=*/false, /*early=*/true),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k266_366_move_right)});
