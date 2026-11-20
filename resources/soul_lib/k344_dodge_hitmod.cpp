// K344 闪避+受伤增幅族 —— effect 352（1 宏 / 1 行）
//（噬神狂天 2728 args[500,1,200]；号段 5000-5009，worktree ../agents/k344，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 352，2026-10-21）：
//   「每回合都有50%概率闪避对手攻击，每回合受到的伤害增加20%」
//   → db[0]=500 = 闪避概率千分比（50% 字面互证，151 千分比同款）；db[2]=200 = 受伤
//   增幅千分比（20% 互证）；db[1]=1 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b352\b 无魂印硬编码；②effect_info
//    352 无模板行；③soul_lib 全 worktree/registry 无 352 注册——真缺口。
// 实现：
//   ① 闪避半边（K164 266 命中压制通道）：BATTLE_ROUND_START 桶（ws 每回合 reset →
//   每回合重写）→ ws.hit_rate_mod_attack[对手] = 1 - db[0]/1000（对手每刀 50% 落空
//   = 逐刀 50% 闪避——"每回合都有50%概率闪避"落地为常驻乘区，口径记档）。
//   ② 受伤增幅半边（2026-10-21 白班落地，修正夜跑"引擎无正向通道"误诊）：
//   Calculation::applyDamageReduction 的加算槽算术**本就对称钳 ±100**（负值=增幅，
//   calculation.h:71-76，add_sum=-20 → ×1.2、=-100 → ×2）——同节点写
//   ws.damage_reduce_add[owner][首个空槽] = -db[2]/10（200‰ → -20）即可，零内核改动；
//   ws 每回合 reset → 每回合重写（退场残留至多当回合，K283 damage_add_pct 同款口径）。
//   db[2] 按"在位即消费"纪律（K314/K340 坑集）：显式 0 = 关闸不写。
// 注册：SOUL_PROGRAM(352) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_5000_dodge_hitmod.cpp（号段 5000-5009）。

#include "common.h"

namespace {

constexpr int kMarkId = 352;

// ROUND_START：闪避乘区重写。
EffectResult k344_352_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int opp = 1 - owner;
    if (ctx->getPet(opp).hp <= 0 || ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    int permille = 500;
    int amp_permille = 200;   // 受伤增幅千分比（在位即消费：db[2] 缺省 200，显式 0 关闸）
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 0 && db[0] > 0) {
            permille = db[0];
        }
        if (db.size() > 2) {
            amp_permille = db[2];
        }
    }
    // 对手命中乘区 = 1 - 闪避率（K164 266 命中压制通道反转位）。
    ctx->ws.hit_rate_mod_attack[opp] = 1.0f - static_cast<float>(permille) / 1000.0f;
    // 受伤增幅半边：加算减伤槽写负值 = 增幅乘区（首个空槽，diwang/crawford 同款惯例；
    // applyDamageReduction 对 add_sum 对称钳 ±100）。
    if (amp_permille > 0) {
        int(&reduce)[4] = ctx->ws.damage_reduce_add[owner];
        for (int i = 0; i < 4; ++i) {
            if (reduce[i] == 0) {
                reduce[i] = -amp_permille / 10;
                break;
            }
        }
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（352 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k344_352_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
