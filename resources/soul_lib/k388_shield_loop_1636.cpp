// K388 护盾自循环族 —— effect 1636（1 宏 / 2 行，环·回响 4043）
//（接管实现 2026-10-24，worktree ../agents/k388；号段 7410-7419，a26-kprod）
//
// 行清单（tools/soul_effects_dump.py 1636，2026-10-24）：
//   row1296 本体 args[100,150,600,50,50,1]：「战斗阶段结束时，为自身附加已损失体力值
//     100%的护盾，自身体力低于对手时效果升至150%，最高600点；自身攻击技能命中后恢复
//     自身当前护盾值50%的体力，自身属性技能命中后附加自身当前护盾值50%的百分比伤害
//     （boss无效）」
//   row2135 伙伴行 args[150,200,3000,100,100,1]：同文 100%→150%、150%→200%、
//     50%→100%（tips 漏"最高3000点"字样，按行参 db2=3000 落上限，记档）。
//
// ⚠️ 三查记录（a79-jprod 切片核查 2026-10-24 / a26-kprod 动工同日复核）：
//   ①内核 grep 无 1636 魂印行为硬编码；②effect_info 1636 异文（玫瑰卡牌模板
//   ——tips 权威）；③registry soul 无 1636、soul_lib 全库 grep 无同 id 注册——真缺口。
//
// 行参解码（两行同构互锚，db_at 原序）：
//   db[0]=①护盾率（100/150）、db[1]=①低于对手升档率（150/200）、db[2]=①上限
//   （600/3000）、db[3]=②攻击命中回复率（50/100）、db[4]=②属性技附伤率（50/100）、
//   db[5]=1 旗（不消费——K1 纪律）。
//
// 实现（板行处方）：
//   ① BATTLE_ROUND_END：lost = max-hp → shield = min(lost×率, db2)；率 = 自身 hp
//     **严格低于**对手在场宠 → max(db0,db1)，否则 db0。授予走 ShieldBank 魂印专属盾
//     约定（kSoulShieldPriority + 同 source_id refresh——"同一精灵的魂印专属盾至多
//     一个"，shield_bank.h 约定；"每回合刷新的盾"= 刷新不并排叠加，记档）。
//     满体（lost≤0）不附加。
//   ② 攻击命中回复：DETECT 攻方票（K174 818 直系）——d.attackerId==own + isRed +
//     final>0（"命中后"：miss/零伤不回复）→ heal_amount(当前护盾 total×db3%)，
//     回复源=护盾余量（与 K384 吸取形不同：不做等量扣除）。TEAMB 绑定常驻票 +
//     onstage 现场查。
//   ②' 属性技命中附伤：FIRST/SECOND_ON_SKILL_HIT 声明桶（K231 1396 直系——属性技
//     无伤害相位进不了 DETECT，G5 口径）→ exec HIT 门 + skill_type_view Attribute 门
//     → deal_pink_damage(对手, 当前护盾 total×db4%, PERCENT_VALUE)。
// 注册：SOUL_PROGRAM(1636) 四节点（early arm + ROUND_END + 双 ON_SKILL_HIT），
// 总数 +1（只增不改）；tips 尾注（boss无效）→ 全节点 boss_invalid=true（boss 局
// 节点不注册 → DETECT 票不武装/ROUND_END 不授盾）。
// 场景回归：test/scenario/scenario_7410_shield_loop_1636.cpp（7410-7419）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1636;
constexpr int kShieldSource = 1636;        // 魂印专属盾 source_id（refresh 同源）
constexpr int kWiredKeyBase = 990388000;   // +owner：DETECT 票已武装旗标（818 同款）

// ── 宿主行参数（db_at 读 owned_int_args 原序——K384/K187 方案 B 同款）──────────
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

// ── early arm：武装②攻击命中回复票（DETECT 攻方桶，TEAMB 常驻）────────────────
EffectResult k388_1636_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // early 双触发防重（818/K384 同款）
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->heal_amount) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 「命中后」：miss/零伤不回复（818 同门）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;   // 换下场不回复（DETECT 桶 TEAM 绑定，现场查在场）
            }
            const int slot = c->find_pet_with_soulmark(owner, kMarkId);
            if (slot < 0 || slot > 5) {
                return;
            }
            const auto& db =
                c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
            const int heal_rate = db.size() > 3 ? db[3] : 50;
            const int shield = c->getPet(owner).shield_bank_.total();
            if (heal_rate <= 0 || shield <= 0) {
                return;   // 回复源=当前护盾值，无盾不回复
            }
            (void)g_core->heal_amount(c, owner, shield * heal_rate / 100);
        });
    return EffectResult::kOk;
}

// ── ① ROUND_END：已损体力×率 护盾（低于对手升档、上限封顶、同源刷新）──────────
EffectResult k388_1636_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int rate_base = db_at(row, 0, 100);
    const int rate_up = db_at(row, 1, rate_base);
    const int cap = db_at(row, 2, 600);
    const ElfPet& foe =
        ctx->seerRobot[1 - owner].elfPets[ctx->on_stage[1 - owner]];
    // 自身体力**低于**对手 → 升档率（处方：分叉取 max(db0,db1)）
    const int rate = (self.hp < foe.hp) ? std::max(rate_base, rate_up) : rate_base;
    const int max_hp = resolve_pet_max_hp(self);
    const int lost = max_hp - self.hp;
    if (lost <= 0) {
        return EffectResult::kOk;   // 满体无"已损失"，不附加
    }
    int amount = lost * rate / 100;
    if (cap > 0) {
        amount = std::min(amount, cap);
    }
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    // 魂印专属盾约定：同 source_id 刷新（不并排叠加）；被耗空（槽已移除）→ 重加。
    auto& bank = self.shield_bank_;
    bank.refresh_source(kShieldSource, amount);
    if (bank.quantity_of(kShieldSource) <= 0) {
        bank.add(ShieldBank::kSoulShieldPriority, amount, kShieldSource,
                 /*duration=*/0);
    }
    return EffectResult::kOk;
}

// ── ②' 属性技命中：附加当前护盾值×db4% 百分比伤害（粉）───────────────────────
EffectResult k388_1636_on_hit(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->ws.skill_exec_result[owner] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // 「命中后」（1396 同门：非 HIT 不触发）
    }
    if (ctx->ws.skill_type_view[owner] != static_cast<int>(SkillType::Attribute)) {
        return EffectResult::kOk;   // 只吃属性技分支（攻击技分支=DETECT 回复票）
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int pink_rate = db_at(row, 4, 50);
    const int shield = ctx->getPet(owner).shield_bank_.total();
    if (pink_rate <= 0 || shield <= 0) {
        return EffectResult::kOk;   // 无盾无从附加
    }
    (void)g_core->deal_pink_damage(ctx, 1 - owner, shield * pink_rate / 100,
                                   DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1636 入账，只增不改）──────
// tips 尾注（boss无效）→ 全节点 boss_invalid=true。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k388_1636_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k388_1636_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_ON_SKILL_HIT, &k388_1636_on_hit,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_SECOND_ON_SKILL_HIT, &k388_1636_on_hit,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
