// K349 回合条件护盾/属性族 —— effect 941/951（2 宏 / 2 行）
//（941 奥斯里安 3331 args[250,1,1]、951 天玄界皇 3346 args[1,200,1]；
//  号段 5190-5199，worktree k349，2026-10-22）
//
// 行清单（tools/soul_effects_dump.py 941 951，2026-10-22）：
//   941「每回合开始时为自身附加250点护盾；战斗阶段结束时若自身存在护盾则使对手
//        全属性-1（BOSS无效）」→ db[0]=250=护盾值 / db[1]=1=全属性降级数 /
//        db[2]=1 废位不消费。
//   951「回合开始时若自身体力高于对手则自身全属性+1（BOSS无效）；战斗阶段结束时
//        恢复200点体力并造成等量固定伤害」→ db[0]=1=全属性提升级数 /
//        db[1]=200=回复量与等量固伤 / db[2]=1 废位不消费。
//   「全属性」= 6 槽含命中（用户 2026-09-18 口径；chaos_sorensen.cpp:106）。
//
// ⚠️ 三查记录（a15-kprod 2026-10-22）：
//   ① 内核 grep 941/951 无魂印行为硬编码；
//   ② effect_info 941/951 均为技能空间异文（「出手时有概率使对手{0}…」等），
//      不同注册空间；
//   ③ registry soul 与全 feat 分支 soul_lib 并集均无 941/951 注册——真缺口。
// 实现形态（套路目录 §零→②原语组合）：
//   941 ① ROUND_START：shield_bank_.add(kSoulShieldPriority, db[0], source=941,
//      duration=0)——"每回合开始时附加"按**叠加**落地（duration=0 永久，破盾前持续；
//      canglan_pink/k173 同通道）。
//      ② ROUND_END：shield_bank_.total() > 0 → 对手 6 槽各 stat_drop(db[1])
//      （弱化原语自免弱，k242 全属性-1 同款）。
//   951 ① ROUND_START：self.hp > foe.hp → 自身 6 槽各 stat_change(db[0])
//      （自增益通道，±6 原语内钳）。
//      ② ROUND_END：heal_amount(self, db[1]) + deal_pink_damage(对手, db[1], FIXED)
//      （"造成等量固定伤害"= 与回复同值，k237/k242 同原语）。
//   ⚠️ 行参一律 db_at 读 owned_int_args 原序；显式 0 = 关闸不 fallback。
// ⚠️ boss_invalid 按**子句**（tips 分号切）而非按整行：
//     941 ROUND_START（无尾注）false / ROUND_END（带 BOSS无效）true；
//     951 ROUND_START（带 BOSS无效）true / ROUND_END（无尾注）false。
// 先例：canglan_pink 2343（shield_bank_ 魂印盾）、k173_order_branch_shield 809
//   （ROUND_END 挂盾）、k242_hpbranch_fear_drain 1414（回合末全属性 stat_drop + HP
//   对照）、k196_round_stat_moves 390（ROUND_START 全属性自增益）、k237 665/heal_amount。
// 场景回归：test/scenario/scenario_5190_round_shield_stat.cpp（号段 5190-5199）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark941 = 941;
constexpr int kMark951 = 951;
constexpr int kAllStatCount = 6;   // 全属性 = 攻/特攻/防/特防/速/命中

struct HostRow {
    const std::vector<int>* db = nullptr;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, int owner, int mark_id) {
    HostRow row;
    if (!ctx || owner < 0 || owner > 1) {
        return row;
    }
    const int slot = ctx->find_pet_with_soulmark(owner, mark_id);
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

// ── 941 ─────────────────────────────────────────────────────────────────────
EffectResult k349_941_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark941)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, owner, kMark941);
    const int amount = db_at(row, 0, 250);
    if (amount > 0) {
        self.shield_bank_.add(ShieldBank::kSoulShieldPriority, amount,
                              /*source_id=*/kMark941, /*duration=*/0);
    }
    return EffectResult::kOk;
}

EffectResult k349_941_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark941)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    const int opp = ctx->opponent(owner);
    if (self.hp <= 0 || ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;
    }
    if (self.shield_bank_.total() <= 0) {
        return EffectResult::kOk;   // 战斗阶段结束时若自身存在护盾
    }
    const HostRow row = host_row(ctx, owner, kMark941);
    const int drop = db_at(row, 1, 1);   // 在位即消费：显式 0 = 本行关闸
    if (drop <= 0) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < kAllStatCount; ++i) {
        (void)g_core->stat_drop(ctx, opp, i, drop, owner);
    }
    return EffectResult::kOk;
}

// ── 951 ─────────────────────────────────────────────────────────────────────
EffectResult k349_951_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark951)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    ElfPet& foe = ctx->getPet(ctx->opponent(owner));
    if (self.hp <= 0 || foe.hp <= 0 || self.hp <= foe.hp) {
        return EffectResult::kOk;   // 自身体力高于对手
    }
    const HostRow row = host_row(ctx, owner, kMark951);
    const int up = db_at(row, 0, 1);   // 在位即消费：显式 0 = 关闸
    if (up <= 0) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < kAllStatCount; ++i) {
        (void)g_core->stat_change(ctx, owner, i, up);
    }
    return EffectResult::kOk;
}

EffectResult k349_951_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark951)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    const int opp = ctx->opponent(owner);
    if (self.hp <= 0 || ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, owner, kMark951);
    const int amount = db_at(row, 1, 200);
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->heal_amount(ctx, owner, amount);
    (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +2（941/951 入账，只增不改）──
// ⚠️ boss_invalid 按子句尾注分别标注（见文件头）。
SOUL_PROGRAM(kMark941,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k349_941_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_ROUND_END, &k349_941_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});

SOUL_PROGRAM(kMark951,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k349_951_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k349_951_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
