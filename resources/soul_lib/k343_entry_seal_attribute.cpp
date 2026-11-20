// K343 入场封属性技族 —— effect 289（1 宏 / 1 行）
//（苍魂·青龙 2545 args[2,1]；号段 4990-4999，worktree ../agents/k343，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 289，2026-10-21）：
//   「上场后2回合内使对手属性技能无效 （只触发一次 ）」
//   → db[0]=2 = 封印回合数（"2回合内"字面自证）；db[1]=1 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b289\b 无魂印硬编码；②effect_info
//    289 无模板行；③soul_lib 全 worktree/registry 无 289 注册——真缺口。
// 实现（板行处方：K231 失神 SEALED 通道同型——seal_skill attribute=true）：
//   OPERATION_ENTER_EXIT_STAGE 节点（early）：只触发一次（plugin_storage 全战旗标，
//   再入场不重挂）→ seal_skill(source=owner, target=对手, effect_id=289,
//   attribute=true, attack=false, count=0（时长型）, duration_rounds=db[0],
//   penetrable=true, ON_STAGE, chance_pct=100)。boss 无注记 → 无门。
// 注册：SOUL_PROGRAM(289) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4990_entry_seal_attribute.cpp（号段 4990-4999）。

#include "common.h"

namespace {

constexpr int kMarkId = 289;
constexpr int kOnceKeyBase = 990843000;   // +owner：只触发一次旗标

// 入场：一次性挂对手属性技封印 db[0] 回合。
EffectResult k343_289_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int once_key = kOnceKeyBase + owner;
    if (ctx->plugin_storage.count(once_key)) {
        return EffectResult::kOk;   // "只触发一次"
    }
    ctx->plugin_storage.emplace(once_key, 1);
    int rounds = 2;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 0 && db[0] > 0) {
            rounds = db[0];
        }
    }
    (void)g_core->seal_skill(ctx, /*source=*/owner, /*target=*/1 - owner,
                             /*effect_id=*/kMarkId, /*attribute=*/true,
                             /*attack=*/false, /*count=*/0,
                             /*duration_rounds=*/rounds, /*penetrable=*/true,
                             /*source_slot=*/-1, EffectScope::ON_STAGE,
                             /*hit_invalid=*/false, /*chance_pct=*/100,
                             /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（289 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k343_289_enter,
                        /*once=*/false, /*early=*/true)});
