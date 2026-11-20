// K240 连击同型增伤/消除回合类族 —— effect 776（1 宏 / 1 行）
//（伽罗·八部天龙 3117；author k-prod-6 2026-10-08 立单，号段 3840-3849）
//
// 行清单（tools/soul_effects_dump.py 776，2026-10-12）：
//   row466 args[50,1]（kind[1,8]）
//   「连续使用攻击技能时，技能造成的伤害提高50%；连续使用属性技能时，消除对手
//    回合类效果（boss无效）」
//   → db[0]=增伤 50；db[1] 两行文恒 1、无对应子句——不消费，记档。
//
// 实现（EVENT_SKILL_USED 窗，K399 同窗：own + 声明类型 used_attribute）：
//   soulmark_storage 连击账本记"上次使用的技能类型"（0=攻击/1=属性，随宠存活——
//   官方无重置文句，切换/被控不断连（被控不发 SKILL_USED，无异型使用插入）——记档）：
//   · 本次类型 == 上次类型 == 攻击 → ws.damage_add_pct[owner] += db[0]
//     （battleContext.h:1594 钦定通道：AMP 加算槽 final×(100+pct)/100，攻击方桶；
//     本效果每次攻击使用时点重写、ws 每回合 reset——正是内核注释"如 776"的预期形态）；
//   · 本次类型 == 上次类型 == 属性 → break_round_effects(对手)（K399 三态原语）。
//   账本无条件更新为本次类型（异型使用即断连）。
// boss 无效标注：tips 尾注 → arm 节点 boss_invalid。
// ⚠️ 三查记录（k-prod-5 2026-10-12）：①内核 battleContext.h:1594 注释**点名 776** 走
//    ws 增伤槽（"下N回合伤害翻倍"类），无行为硬编码；②effect_info 776 同号模板不同文
//    （args_num=1"下{0}回合自身造成的攻击伤害翻倍"——错位行，按行 tips 落地，且幅度
//    语义为提高 50% 非翻倍）；③soul_lib/registry 无 776 注册——真缺口。
// 先例：k399（EVENT_SKILL_USED 窗+break_round_effects）、k225（soulmark_storage
//   runtime 形）、H10（ws 增伤槽消费面）。
// 场景回归：test/scenario/scenario_3840_streak_amp_break.cpp（号段 3840-3849）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 776;
constexpr int kWiredKeyBase = 990760500;   // +owner：watcher 已挂旗标

struct K240Runtime {
    bool watcher_armed = false;
    int last_type = -1;   // -1=未使用 0=攻击 1=属性（连击账本，随宠存活）
};

K240Runtime& k240_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K240Runtime{};
    }
    return std::any_cast<K240Runtime&>(storage[kMarkId]);
}

int k240_read_pct(BattleContext* ctx, int owner) {
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return 0;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    return db.size() > 0 ? db[0] : 0;
}

EffectResult k240_776_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    K240Runtime& rt = k240_runtime(ctx, owner);
    if (rt.watcher_armed) {
        return EffectResult::kOk;
    }
    rt.watcher_armed = true;
    const int pct = k240_read_pct(ctx, owner);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, pct](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, kMarkId)) {
                return;
            }
            const int type = ev.used_attribute ? 1 : 0;   // 声明类型（K399 同读法）
            K240Runtime& rt = k240_runtime(wctx, owner);
            if (rt.last_type == type) {
                if (type == 0 && pct > 0) {
                    // 连续攻击：本次伤害 +pct%（AMP 加算槽，使用时点重写、ws 回合 reset）
                    wctx->ws.damage_add_pct[owner] += pct;
                } else if (type == 1) {
                    // 连续属性：消除对手回合类效果（三态原语，返回值不分支）
                    (void)g_core->break_round_effects(wctx, wctx->opponent(owner));
                }
            }
            rt.last_type = type;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（776 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k240_776_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
