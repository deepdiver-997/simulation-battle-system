// K387 先后手吸取回复+承伤转护盾族 —— effect 1618（1 宏 / 2 行）
//（地葬界神 4024 row1282/row1539；号段 7400-7409，worktree ../agents/k387，2026-10-24）
//
// 行清单（tools/soul_effects_dump.py 1618，2026-10-24 a79-jprod 核）：
//   1618 row1282 args[3,3,50,400,50,400,1]（kind=[2,5,14]）本体行：
//     「自身所有技能先出手时{吸取}对手最大{体力}的1/3，后出手时恢复自身最大体力的1/3
//      并造成等量百分比伤害；|自身受到攻击伤害后利用大地将其转换为自身的力量：战斗
//      阶段结束时为自身附加所受攻击伤害值50%的等量护盾（最高400点），下回合自身使用
//      攻击技能时附加所受攻击伤害值50%的百分比伤害（最高400点）（boss无效）」
//   1618 row1539 args[3,3,100,400,100,400,1] 伙伴行（六界神谕）：转化率 50%→100%，
//     其余同构互锚。
//
// 行参解码（两行互锚，board 处方同）：db[0]=①先手吸取分母（1/db[0] 对手最大体力）、
//   db[1]=①后手回复分母（1/db[1] 自身最大体力）、db[2]=②护盾转化率%（50/100）、
//   db[3]=②护盾上限（400 两行同）、db[4]=②下回合附伤转化率%、db[5]=②附伤上限、
//   db[6]=旗（不消费，K1 纪律）。effect_info 1618 异文（PP 归零模板）→ tips 权威。
//
// 实现（板行处方逐子句；K181 617 先后手对 + K262 369 DETECT 票 + K302 735 吸取拆原语
//   + K187 2474/地葬盾 shield_bank 口径）：
//   ① 先后手对（FIRST/SECOND_SKILL_EFFECT 两节点，桶按 (state,持印者) 配对——持印者
//     先手时 FIRST 桶跑、后手时 SECOND 桶跑，天然就是"先/后出手时"判据，无需读 pr）：
//     · FIRST（先出手）：吸取对手 maxHP/db[0]——dmg=min(maxHP/db[0], 对手当前血)
//       （吸取取不了比血多的，K302 口径）→ deal_pink(对手, dmg, FIXED) +
//       heal_amount(自身, dmg)（按预取量等量恢复；粉伤被减免时恢复不缩，K302 记档）。
//     · SECOND（后出手）：heal = maxHP/db[1] → heal_amount(自身, heal) +
//       deal_pink(对手, heal, PERCENT_VALUE)（"等量百分比伤害"——量=回复量、种类取
//       百分比粉伤通道；与 ① FIXED 吸取的差异记档）。
//   ② 承伤转护盾（两段）：
//     · DETECT 票（early arm 挂注册，K262 同款）：持印者**承红伤**（d.attackerId==opp、
//       d.isRed、d.final>0）→ 账本 ledger(990161800+owner) += d.final；
//       持印者**打红伤**（d.attackerId==owner）且戳有效（roundCount<=until）→
//       deal_pink(对手, stamp, PERCENT_VALUE) 一次性、清戳。
//     · ROUND_END 节点（"战斗阶段结束时"，K302 同桶）：ledger>0 →
//       shield = min(db[3], ledger×db[2]/100) → shield_bank_.add(kSoulShieldPriority,
//       shield, source_id=kMark, duration=0)；stamp = min(db[5], ledger×db[4]/100)、
//       until = roundCount+1（"下回合"窗）；ledger 清零（每回合承伤各转化一次）。
//       记档：下回合未消费的旧戳被新回合的戳覆盖（同键单槽）。
//   boss 标注（标注单位 = tips 按 | 分句）：①（先/后手吸取回复）无尾注 → 节点不置位；
//     ②（承伤转护盾/下回合附伤）带（boss无效）→ DETECT 票 arm 与 ROUND_END 节点置位。
//
// plugin_storage 键（grep 全库无撞）：
//   990161800 + owner = ②本回合承伤账本 ledger
//   990161801 + owner = ②下回合附伤戳 stamp
//   990161802 + owner = ②戳有效 until 回合
//   990161830 + owner = DETECT 票已接线旗（early 双触发防重，K262 同款）
//
// ⚠️ 三查记录（2026-10-24，worktree ../agents/k387）：①内核 grep 1618 无魂印行为
//    硬编码；②effect_icon kind=[2,5,14]、tips 为权威文（dump 原文已核，effect_info
//    1618 异文不取）；③registry soul 集合无 1618、soul_lib 全库 grep 无同 id 注册
//    ——真缺口。切片：2 行 args 全量见上，[own,opp,slot] 绑定后 db_at 原序自洽。
// 先例：k181_order_immune_heal 617（FIRST/SECOND 对 + maxHP 分母回复）、k262 369
//   （DETECT 票 + is_red/attackerId 分支 + early arm 防重）、k302 735（吸取拆
//   deal_pink+heal_amount）、k187 2474 / hundun_diwang_2411（shield_bank_.add 口径）、
//   resolve_pet_max_hp（soul_lib/common.h）。
// 场景回归：test/scenario/scenario_7400_order_drain_shield.cpp（号段 7400-7409）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1618;
constexpr int kLedgerBase = 990161800;   // +owner：本回合承伤账本
constexpr int kStampBase = 990161801;    // +owner：下回合附伤戳
constexpr int kUntilBase = 990161802;    // +owner：戳 until 回合
constexpr int kWiredBase = 990161830;    // +owner：DETECT 票接线旗

int stor_get(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
}

void stor_set(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// ── ①FIRST：先出手 → 吸取对手 1/db[0] 最大体力（deal_pink FIXED + 等量回复）────
EffectResult k387_first_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->deal_pink_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    ElfPet& foe = ctx->getPet(opp);
    ElfPet& host = ctx->getPet(owner);
    if (foe.hp <= 0 || host.hp <= 0) {
        return EffectResult::kOk;
    }
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    const int denom = db.size() > 0 ? db[0] : 3;
    if (denom <= 0) {
        return EffectResult::kOk;
    }
    const int dmg = std::min(resolve_pet_max_hp(foe) / denom, foe.hp);
    if (dmg <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, opp, dmg, DamageKind::FIXED, owner);
    (void)g_core->heal_amount(ctx, owner, dmg);
    return EffectResult::kOk;
}

// ── ①SECOND：后出手 → 回复自身 1/db[1] 最大体力 + 等量百分比伤害 ─────────────
EffectResult k387_second_heal_amp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->deal_pink_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    ElfPet& foe = ctx->getPet(opp);
    ElfPet& host = ctx->getPet(owner);
    if (host.hp <= 0) {
        return EffectResult::kOk;
    }
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    const int denom = db.size() > 1 ? db[1] : 3;
    if (denom <= 0) {
        return EffectResult::kOk;
    }
    const int heal = resolve_pet_max_hp(host) / denom;
    if (heal <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->heal_amount(ctx, owner, heal);
    if (foe.hp > 0) {
        (void)g_core->deal_pink_damage(ctx, opp, heal, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── ②ROUND_END：承伤账本 → 护盾（≤db[3]）+ 下回合附伤戳（≤db[5]）────────────
EffectResult k387_round_end_convert(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    const int shield_rate = db.size() > 2 ? db[2] : 50;
    const int shield_cap = db.size() > 3 ? db[3] : 400;
    const int stamp_rate = db.size() > 4 ? db[4] : 50;
    const int stamp_cap = db.size() > 5 ? db[5] : 400;
    const int ledger = stor_get(ctx, kLedgerBase + owner);
    if (ledger <= 0) {
        return EffectResult::kOk;
    }
    if (shield_rate > 0) {
        const int shield = std::min(shield_cap, ledger * shield_rate / 100);
        if (shield > 0) {
            ctx->getPet(owner).shield_bank_.add(ShieldBank::kSoulShieldPriority, shield,
                                                /*source_id=*/kMarkId, /*duration=*/0);
        }
    }
    if (stamp_rate > 0) {
        const int stamp = std::min(stamp_cap, ledger * stamp_rate / 100);
        if (stamp > 0) {
            stor_set(ctx, kStampBase + owner, stamp);
            stor_set(ctx, kUntilBase + owner, ctx->roundCount + 1);
        }
    }
    stor_set(ctx, kLedgerBase + owner, 0);   // 每回合承伤各转化一次
    return EffectResult::kOk;
}

// ── ②arm（early）：挂 DETECT 票——承伤记账 + 下回合攻击附伤戳消费 ────────────
EffectResult k387_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int wired = kWiredBase + owner;
    if (ctx->plugin_storage.count(wired)) {
        return EffectResult::kOk;   // early 双触发防重（K262 同款）
    }
    ctx->plugin_storage.emplace(wired, 1);

    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (!d.isRed || d.final <= 0) {
                return;
            }
            const int opp = c->opponent(owner);
            if (d.attackerId == opp) {
                // 承伤记账（"受到攻击伤害后"）
                stor_set(c, kLedgerBase + owner, stor_get(c, kLedgerBase + owner) + d.final);
                return;
            }
            if (d.attackerId == owner) {
                // 下回合攻击附伤戳消费（一次性；"下回合"窗由 until 圈定）
                const int stamp = stor_get(c, kStampBase + owner);
                const int until = stor_get(c, kUntilBase + owner);
                if (stamp > 0 && until >= c->roundCount) {
                    if (g_core && g_core->deal_pink_damage) {
                        (void)g_core->deal_pink_damage(c, opp, stamp,
                                                       DamageKind::PERCENT_VALUE, owner);
                    }
                    stor_set(c, kStampBase + owner, 0);
                }
                return;
            }
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1618 入账，只增不改）──────
// boss 标注单位 = tips 按 | 分句：①（先/后手吸取回复）无尾注 → 两节点不置位；
// ②（承伤转护盾/下回合附伤）带（boss无效）→ DETECT 票 arm 与 ROUND_END 节点置位。
SOUL_PROGRAM(kMarkId,
             {SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT, &k387_first_drain),
              SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT, &k387_second_heal_amp),
              SoulMarkNodeRef{State::BATTLE_ROUND_END, &k387_round_end_convert,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true},
              SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k387_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
