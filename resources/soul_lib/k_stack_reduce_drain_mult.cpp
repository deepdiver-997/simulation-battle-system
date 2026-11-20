// K351 叠减伤/吸血与概率倍增族 —— effect 1059/1060（2 宏 / 2 行）
//（1059 幻境界皇 3460 args[35,15,65,50,100]、1060 恩菲 3459 args[15,5,4,50,50,1]；
//  号段 5240-5249，worktree k351，2026-10-22）
//
// 行清单（tools/soul_effects_dump.py 1059 1060，2026-10-22）：
//   1059「[叠加效果]: 回合受到攻击伤害减少35%，之后每回合受到攻击伤害减少量增加15%，
//         最高65%；[恢复效果]: 攻击造成伤害的50%恢复自身体力，若打出致命一击则造成
//         伤害的100%恢复自身体力」
//        → db[0]=35 起始减伤 / db[1]=15 每回合增量 / db[2]=65 上限 /
//          db[3]=50 吸血% / db[4]=100 暴击吸血%。
//   1060「15%的概率造成5倍伤害，对手每有一只精灵存活，概率提高4%；每回合恢复50点
//         体力，己方每有一只精灵存活则额外恢复50点体力（BOSS无效）」
//        → db[0]=15 起始概率 / db[1]=5 伤害倍数 / db[2]=4 每个存活对手的概率增量 /
//          db[3]=50 每回合回复 / db[4]=50 每个存活己方的额外回复 / db[5]=1 废位。
//
// ⚠️ 三查记录（a15-kprod 2026-10-22）：
//   ① 内核 grep 1059/1060 无魂印行为硬编码；
//   ② effect_info 1059/1060 均为技能空间异文（「消耗自身全部体力…」等），不同空间；
//   ③ registry soul 与全 feat 分支 soul_lib 并集均无 1059/1060 注册——真缺口。
// 实现形态（套路目录 §零→②原语组合）：
//   1059 ① ROUND_START：本次上场回合计数 n（on_stage_storage）→
//        reduce = min(db[0] + db[1]×(n-1), db[2]) → ws.damage_reduce_add[owner] 首个
//        空槽（加算减伤槽；ws 每回合 reset → 每回合重写，diwang/k344 同套路）。
//        ② DETECT（own 红伤落地）：heal_amount(owner, final×db[3或db[4]]/100)
//        （d.isCrit 取 db[4]，否则 db[3]）。
//   1060 ① BATTLE_FIRST/SECOND_BEFORE_SKILL_HIT（镜像注册；回调用 flow_mover 判本次
//        出手方 == owner）：攻击技 → 概率 = db[0] + db[2]×存活对手数 → roll →
//        ws.damage_add_extra_mul[owner] 首个空槽 = (db[1]-1)×100（AMP_EXTRA 乘算，
//        5 倍 = +400%；battleWorkspace.h:299）。
//        ② ROUND_END：heal_amount(owner, db[3] + db[4]×存活己方数)。
//   ⚠️ 行参一律 db_at 读 owned_int_args 原序；显式 0 = 关闸不 fallback。
//   ⚠️ ws 槽每回合 reset → 1059 减伤与 1060 倍率都只作用于当回合（官方为回合类语义）。
// ⚠️ boss：1059 tips 无尾注 → 全节点 false；1060 tips 带「（BOSS无效）」→ 全节点 true。
// 先例：k344_dodge_hitmod 352（ws.damage_reduce_add 首个空槽）、k289_boost_damage_amp
//   659（damage_add_pct 同族）、encarlos_light_3577 1164（FIRST/SECOND_BEFORE_SKILL_HIT
//   镜像 + flow_mover 判出手方）、k237 665（heal_amount）。
// 场景回归：test/scenario/scenario_5240_stack_reduce_drain_mult.cpp（号段 5240-5249）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark1059 = 1059;
constexpr int kMark1060 = 1060;
constexpr int kCounter1059 = 990105900;   // on_stage_storage：本次上场回合数
constexpr int kPartySize = 6;

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

int alive_count(const BattleContext* ctx, int side) {
    int n = 0;
    for (int i = 0; i < kPartySize; ++i) {
        if (ctx->seerRobot[side].elfPets[i].hp > 0) {
            ++n;
        }
    }
    return n;
}

void add_reduce_slot(BattleContext* ctx, int owner, int pct) {
    if (pct <= 0) {
        return;
    }
    int (&slots)[4] = ctx->ws.damage_reduce_add[owner];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = pct;
            return;
        }
    }
}

void add_extra_mul_slot(BattleContext* ctx, int owner, int pct) {
    if (pct <= 0) {
        return;
    }
    int (&slots)[4] = ctx->ws.damage_add_extra_mul[owner];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = pct;
            return;
        }
    }
}

// 当前出手流程的 mover（FIRST/SECOND 通算；非命中前时点返回 -1）。
// 与 encarlos_light_3577.cpp:flow_mover 同款。
int flow_mover(BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    if (ctx->currentState == State::BATTLE_FIRST_SKILL_EFFECT
        || ctx->currentState == State::BATTLE_FIRST_BEFORE_SKILL_HIT
        || ctx->currentState == State::BATTLE_FIRST_ACTION_END) {
        return ctx->is_second_mover(0) ? 1 : 0;
    }
    if (ctx->currentState == State::BATTLE_SECOND_SKILL_EFFECT
        || ctx->currentState == State::BATTLE_SECOND_BEFORE_SKILL_HIT
        || ctx->currentState == State::BATTLE_SECOND_ACTION_END) {
        if (ctx->is_second_mover(0) == ctx->is_second_mover(1)) {
            return -1;
        }
        return ctx->is_second_mover(0) ? 0 : 1;
    }
    return -1;
}

// ── 1059 ────────────────────────────────────────────────────────────────────
EffectResult k351_1059_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark1059)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, owner, kMark1059);
    const int start = db_at(row, 0, 35);
    const int step = db_at(row, 1, 15);
    const int cap = db_at(row, 2, 65);
    // operator[] 先落一个空 any；any_int_or 按缺失语义回退 0，再写回 int。
    int n = any_int_or(self.on_stage_storage[kCounter1059], 0);
    ++n;
    self.on_stage_storage[kCounter1059] = n;
    int reduce = start + step * (n - 1);
    if (cap > 0 && reduce > cap) {
        reduce = cap;
    }
    add_reduce_slot(ctx, owner, reduce);
    return EffectResult::kOk;
}

EffectResult k351_1059_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = 990590000 + owner;   // TEAM 管线已接线旗标
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || !g_core || !g_core->heal_amount || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark1059)) {
                return;
            }
            const HostRow row = host_row(c, owner, kMark1059);
            const int pct = d.isCrit ? db_at(row, 4, 100) : db_at(row, 3, 50);
            if (pct <= 0) {
                return;
            }
            (void)g_core->heal_amount(c, owner, d.final * pct / 100);
        });
    return EffectResult::kOk;
}

// ── 1060 ────────────────────────────────────────────────────────────────────
EffectResult k351_1060_pre_hit(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark1060)) {
        return EffectResult::kOk;
    }
    if (flow_mover(ctx) != owner) {
        return EffectResult::kOk;   // 只有自己出手时才掷
    }
    if (ctx->ws.skill_type_view[owner]
        == static_cast<int>(SkillType::Attribute)) {
        return EffectResult::kOk;   // 只对攻击技
    }
    const HostRow row = host_row(ctx, owner, kMark1060);
    const int base = db_at(row, 0, 15);
    const int mult = db_at(row, 1, 5);
    const int per_foe = db_at(row, 2, 4);
    if (mult <= 1) {
        return EffectResult::kOk;
    }
    int chance = base + per_foe * alive_count(ctx, ctx->opponent(owner));
    if (chance > 100) {
        chance = 100;
    }
    if (chance <= 0 || !roll_percent(chance)) {
        return EffectResult::kOk;
    }
    add_extra_mul_slot(ctx, owner, (mult - 1) * 100);
    return EffectResult::kOk;
}

EffectResult k351_1060_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark1060)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, owner, kMark1060);
    const int base = db_at(row, 3, 50);
    const int per_ally = db_at(row, 4, 50);
    int amount = base + per_ally * alive_count(ctx, owner);
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->heal_amount(ctx, owner, amount);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +2（1059/1060 入账，只增不改）──
SOUL_PROGRAM(kMark1059,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k351_1059_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k351_1059_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false)});

SOUL_PROGRAM(kMark1060,
             {SOUL_NODE(State::BATTLE_FIRST_BEFORE_SKILL_HIT, &k351_1060_pre_hit,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_SECOND_BEFORE_SKILL_HIT, &k351_1060_pre_hit,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k351_1060_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
