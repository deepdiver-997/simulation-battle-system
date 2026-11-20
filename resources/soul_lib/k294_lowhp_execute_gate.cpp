// K294 低体瞬杀门族 —— effect 625（1 宏 / 1 行）
//（马尔修斯（毁灭型） 2882 args[25,1]；号段 4500-4509，worktree ../agents/k294，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 625，2026-10-21）：
//   「回合开始时,若对手体力低于25%,则所有技能必定瞬杀对手（boss无效）」
//   → db[0]=25 = 门限百分比（"低于25%"字面自证）；db[1]=1 语义不明不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b625\b 无魂印硬编码；②effect_info
//    625 技能空间模板不同文；③soul_lib 全 worktree/registry 无 625 注册——真缺口。
// 实现（板行处方：K164 151 Kill 直系换锚——ROUND_START 门 + force_hp_to_zero）：
//   BATTLE_ROUND_START 桶：onstage 且对手存活且 hp×100 < max×db[0]（"低于25%"
//   严格，交叉相乘防整除截断）→ force_hp_to_zero(对手, owner)（秒杀通道：非伤害
//   通道不走免死拦截，帝皇之御口径记档）。boss 无效子句 → 节点 boss_invalid 旗。
//   "所有技能必定瞬杀"的载体语义 = 任意后续攻击必杀——落地为回合开始直接清零
//   （口径选择记档：回合开始门即执行，不挂攻击时点二次门）。
// 注册：SOUL_PROGRAM(625) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4500_lowhp_execute_gate.cpp（号段 4500-4509）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 625;

// ROUND_START：对手低体门 → 瞬杀。
EffectResult k294_625_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->force_hp_to_zero) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    ElfPet& foe = ctx->getPet(opp);
    if (foe.hp <= 0) {
        return EffectResult::kOk;   // 存活门（已倒不重复）
    }
    int pct = 25;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 0 && db[0] > 0) {
            pct = db[0];
        }
    }
    const int foe_max = resolve_pet_max_hp(foe);
    if (foe.hp * 100 >= foe_max * pct) {
        return EffectResult::kOk;   // 未"低于"门限（严格小于，交叉相乘防截断）
    }
    (void)g_core->force_hp_to_zero(ctx, opp, owner);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（625 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k294_625_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
