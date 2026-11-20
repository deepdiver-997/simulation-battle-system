// K391 先后手固伤变档+阈值封技族 —— effect 1497（1 宏 / 2 行）
//（缚影·炼狱魔神 3907 row1174 args[300,400,50,60,300,1] 本体 + row1365
//  args[300,450,50,65,300,1] 伙伴行——高体变档 400→450 / 60→65% 差异；
//  号段 7440-7449，worktree ../agents/k391，2026-10-24）
//
// 行清单（tools/soul_effects_dump.py 1497，2026-10-24）：
//   两行同文：「自身先出手时附加300点固定伤害，若回合开始时体力高于对手则转变为附加
//   {400|450}点固定伤害，自身后出手时下回合受到的伤害减少{50}%，若回合开始时体力高于
//   对手则转变为减少{60|65}%；自身受到的攻击伤害高于300则令对手下次使用的攻击技能
//   失效（boss无效）」
//
// ⚠️ 行≠模板：effect_info 1497 异文（护盾模板）——tips 权威（板行切片核查已核）。
//
// 行参解码（两行同构互锚）：
//   db[0]=先手固伤（300）、db[1]=高体变档固伤（400/450）、db[2]=后手减伤%（50）、
//   db[3]=高体变档减伤%（60/65）、db[4]=封技阈值（300，两行同值）、db[5]=旗（不消费，
//   K1 纪律）。
//
// 落法（板行处方逐子句）：
//   ① 先手固伤：FIRST_SKILL_EFFECT 桶（K387 1618 直系——桶按 (state,持印者) 配对，
//     持印者先手时 FIRST 桶跑，天然"先出手"判据）→ deal_pink(对手, 高体?db1:db0,
//     FIXED)。无技能类型门（"先出手时"字面，属性技先出手照触发，K387 同款）。
//   ①' 后手减伤：SECOND_SKILL_EFFECT 桶 → 写下回合减伤戳（stamp=高体?db3:db2、
//     until=roundCount+1，"下回合"==roundCount 精确窗）；票 REDUCE_PCT/MITIGATE
//     只红伤（K384 1725/K373 856 红伤票形——tips 无"攻击"限定但粉伤管线不进
//     REDUCE_PCT 票面，先例同款记档）。中途写、次回合消费：roundCount==until 判
//     （非 <=——写点在回合中段，防当回合残余消费；roundCount==until 恰=下回合）。
//   ② 阈值封技：DETECT 票（arm@enter early，K383 同位）承红伤 d.base（公式原价——
//     防①'自票削额后级联误判，K383 口径；板行处方写 d.final，取 K383 更稳读法记档）
//     > db4 → seal_skill(owner→opp, attack=true, count=1)（"下次使用的攻击技能失效"
//     次数型，覆盖键刷新不叠加——多次大伤重复插票=刷新，K310/encarlos 3577 同款）。
//   高体门（"回合开始时体力高于对手"）：ENTER+RS 双节点采样（K389 1522 同位——
//     R1 的 RS 在 b.start() 停泊点未执行，enter 首采样；每回合无脑重写）。
//   boss_invalid：tips 尾注挂整串 → 全节点置位；管线票不随节点走 → 票内手动
//     is_boss_challenge 门（K383/hamo 口径，双保险）。
//   行参读取：db_at 读宿主 owned_int_args 原序（方案 B，K257 起夜跑口径）。
//
// 三查记录（2026-10-24，a25-kprod 切片核查 + a82-jprod 动工复核）：
//   ①内核 grep 1497 无魂印行为硬编码；②effect_info 1497 异文、tips 权威（dump 原文
//   已核）；③registry soul 无 1497、soul_lib 全库无 SOUL_PROGRAM(1497)——真缺口。
// 先例：k387 1618（FIRST/SECOND 桶对 + 戳/until 账本）、k384 1725（REDUCE_PCT
//   MITIGATE 红伤票）、k383 1864（DETECT d.base 阈值门 + 手动 boss 门）、
//   chaos_sorensen/encarlos_3577（seal_skill 次数型插票）、k389 1522（ENTER+RS
//   双节点采样，本日定版）。
// 场景回归：test/scenario/scenario_7440_order_fixed_seal_1497.cpp（7440-7449）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1497;
constexpr int kWiredBase = 990391000;     // +owner：票接线旗
constexpr int kHighHpKeyBase = 990391100; // +owner：回合开始"体力高于对手"旗
constexpr int kStampKeyBase = 990391200;  // +owner：下回合减伤戳 %
constexpr int kUntilKeyBase = 990391300;  // +owner：戳生效回合（==roundCount 判）

// ── 宿主行参数（方案 B：db_at 读 owned_int_args 原序，与真实对局一致）────────
struct HostRow {
    const std::vector<int>* db = nullptr;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const SoulNodeArgs& a, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

int flag_of(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

// ── arm（enter/early）：接线 ①' 减伤票 + ② 阈值封技票 ─────────────────────────
EffectResult k391_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWiredBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // 双触发防重（K373 同款）
    }
    ctx->plugin_storage.emplace(key, 1);
    const HostRow row = host_row(ctx, a, kMarkId);
    const int seal_threshold = db_at(row, 4, 0);
    ctx->register_default_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId) || c->is_boss_challenge) {
                return;
            }
            const int until = flag_of(c, kUntilKeyBase + owner);
            const int stamp = flag_of(c, kStampKeyBase + owner);
            if (stamp > 0 && until > 0 && c->roundCount == until) {
                d.final = d.final * (100 - stamp) / 100;
            }
        });
    if (seal_threshold > 0) {
        ctx->register_default_damage_effect(
            DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
            [owner, seal_threshold](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner || !g_core || !g_core->seal_skill) {
                    return;
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.defenderId != owner || !d.isRed) {
                    return;
                }
                if (!onstage_has_soul_mark(c, owner, kMarkId)
                    || c->is_boss_challenge) {
                    return;
                }
                if (d.base <= seal_threshold) {
                    return;   // "高于300"字面：== 不触发（K383 同款字面口径）
                }
                (void)g_core->seal_skill(c, /*source=*/owner, /*target=*/1 - owner,
                                         /*effect_id=*/kMarkId,
                                         /*attribute=*/false, /*attack=*/true,
                                         /*count=*/1, /*duration_rounds=*/0,
                                         /*penetrable=*/true, /*source_slot=*/-1,
                                         EffectScope::ON_STAGE,
                                         /*hit_invalid=*/false, /*chance_pct=*/100,
                                         /*consumed_when_pierced=*/false);
            });
    }
    return EffectResult::kOk;
}

// ── ENTER/RS：高体门采样（每回合无脑重写）────────────────────────────────────
EffectResult k391_sample(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(a.owner);
    ElfPet& foe = ctx->getPet(1 - a.owner);
    const bool high = self.hp > 0 && self.hp > foe.hp;
    ctx->plugin_storage[kHighHpKeyBase + a.owner] = high ? 1 : 0;
    return EffectResult::kOk;
}

// ── ① FIRST：先出手 → 附加固伤（高体变档）────────────────────────────────────
EffectResult k391_first_fixed(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    if (ctx->getPet(opp).hp <= 0 || ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;   // K387 同款存活门
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int base_amt = db_at(row, 0, 0);
    const int high_amt = db_at(row, 1, 0);
    const int amt = flag_of(ctx, kHighHpKeyBase + owner) == 1 ? high_amt : base_amt;
    if (amt <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, opp, amt, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

// ── ①' SECOND：后出手 → 写下回合减伤戳（高体变档）────────────────────────────
EffectResult k391_second_stamp(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(a.owner).hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int base_pct = db_at(row, 2, 0);
    const int high_pct = db_at(row, 3, 0);
    const int pct = flag_of(ctx, kHighHpKeyBase + a.owner) == 1 ? high_pct : base_pct;
    ctx->plugin_storage[kStampKeyBase + a.owner] = pct;
    ctx->plugin_storage[kUntilKeyBase + a.owner] = ctx->roundCount + 1;
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1497 入账，只增不改）──────
SOUL_PROGRAM(kMarkId, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k391_arm,
                             /*once=*/false, /*early=*/true, SoulScope::STAGE,
                             /*boss_invalid=*/true),
                       SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k391_sample,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/true),
                       SOUL_NODE(State::BATTLE_ROUND_START, &k391_sample,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/true),
                       SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k391_first_fixed,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/true),
                       SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k391_second_stamp,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/true)});
