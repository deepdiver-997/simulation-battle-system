// K346 能力提升计数附加固伤族 —— effect 877/911/946（3 宏 / 3 行）
//（合订单：877 暗魇圣瞳·魔眼凯兮 3263 [75,50,1]、911 灵兽·萨尔蒙 3303 [60,1]、
//  946 弗雷尔 3337 [2,100,1]；号段 5080-5089，worktree k346，2026-10-22）
//
// 行清单（tools/soul_effects_dump.py 877 911 946，2026-10-22）：
//   877「攻击技能命中对手时，对手每处于一种能力提升状态则附加75点固定伤害，
//        若对手不处于能力提升状态则有50%的概率使对手石化（BOSS无效）」
//       → db[0]=75=每级固定伤害 / db[1]=50=石化申报概率 / db[2]=1 废位不消费。
//   911「双方每处于一种能力提升状态则自身攻击附加60点固定伤害；
//        战斗阶段结束时若自身处于能力下降状态则解除自身能力下降（BOSS无效）」
//       → db[0]=60=每级固定伤害 / db[1]=1 废位不消费。
//   946「每回合自身速度+2；自身每处于一级速度能力提升则攻击附加100点固定伤害（BOSS无效）」
//       → db[0]=2=每回合速度增量 / db[1]=100=每级速度固伤 / db[2]=1 废位不消费。
// 计数口径：「每处于一种能力提升状态」= 5 项战斗属性（攻/特攻/防/特防/速）中正等级的
//   项数（chaos_sorensen.cpp:143 的 5 项口径；命中槽 5 不计入）。946 只数速度槽。
//
// ⚠️ 三查记录（a15-kprod 2026-10-22）：
//   ① 内核 grep 877/911/946 无魂印行为硬编码（911 仅本体注释里的同号巧合）；
//   ② effect_info 三者均为**技能空间**异文模板（877「先出手时全属性+{0}」等），
//      与魂印行不同空间，不冲突（常识 §一 双注册空间）；
//   ③ registry soul 与全 feat 分支 soul_lib 并集均无 877/911/946 注册——真缺口。
// 实现形态（套路目录 §零→②原语组合；SOP §三.1 条件查询走 DETECT 同步相位）：
//   attach = register_default_damage_effect(DETECT, TEAM)——own 红伤落地门
//   （d.attackerId==owner + isRed + final>0）→ deal_pink_damage(对手, 计数×db, FIXED, owner)
//   （k197 259 / k329 783 同型；DETECT 相位同步读 ability_levels，避免 watcher 晚 drain
//   的现场面过期）。属性技无红伤 → 天然不触发。
//   877 石化分支：对手无提升时按 db[1] 申报概率走 apply_anomaly_checked（现代通道，
//   概率申报版不预掷，K191 双掷点教训）。
//   911 回合末：自己任一槽负等级 → clear_stat_drops(owner)（"解除自身能力下降"）。
//   946 回合首：stat_change(owner, 速度槽4, +db[0])（自增益通道，±6 原语内钳）。
//   ⚠️ 行参一律 db_at 读 owned_int_args 原序（禁止 a.row）；显式 0 关闸不 fallback。
// ⚠️ 三条官方模板均带「（boss无效）」→ 本族全部节点 boss_invalid=true（子句级统一）。
// 先例：k197_hp_pct_attach 259（DETECT 附粉，直系）、k329_curhp_fixed_rider 783
//   （DETECT/固伤）、k191_boost_ice_reversal（申报概率通道）、k_round_stat（ROUND_START
//   自增益）、chaos_sorensen（正等级计数口径）。
// 场景回归：test/scenario/scenario_5080_boostcount_attach.cpp（号段 5080-5089）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark877 = 877;
constexpr int kMark911 = 911;
constexpr int kMark946 = 946;

constexpr int kBattleStatSlots = 5;   // 0攻 1特攻 2防 3特防 4速（命中槽 5 不计）
constexpr int kSpeedSlot = 4;
constexpr int kArmKeyBase = 990846000;  // + mark*10+owner：TEAM 管线已接线旗标

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

// db_at：读宿主魂印 owned_int_args 原序（官方行第 idx+1 位）。
int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

int positive_level_count(const BattleContext* ctx, int side) {
    int n = 0;
    for (int i = 0; i < kBattleStatSlots; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            ++n;
        }
    }
    return n;
}

void attach_fixed(BattleContext* ctx, int owner, int amount) {
    if (amount > 0 && g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, ctx->opponent(owner), amount,
                                       DamageKind::FIXED, owner);
    }
}

// arm 去重：TEAM 管线条目每场只装一次（再登场不重复接线）。
bool wire_once(BattleContext* ctx, int mark_id, int owner) {
    const int key = kArmKeyBase + mark_id * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return false;
    }
    ctx->plugin_storage.emplace(key, 1);
    return true;
}

// ── 877：对手提升项数 × db[0]；无提升时按 db[1] 石化 ─────────────────────────
EffectResult k346_877_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !wire_once(ctx, kMark877, owner)) {
        return EffectResult::kOk;
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || !g_core || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 只随自己的攻击红伤触发（属性技天然无红伤）
            }
            if (!onstage_has_soul_mark(c, owner, kMark877)) {
                return;
            }
            const HostRow row = host_row(c, owner, kMark877);
            const int per = db_at(row, 0, 75);
            const int chance = db_at(row, 1, 50);
            const int opp = c->opponent(owner);
            const int boosts = positive_level_count(c, opp);
            if (boosts > 0) {
                attach_fixed(c, owner, boosts * per);
            } else if (chance > 0 && g_core->apply_anomaly_checked) {
                (void)g_core->apply_anomaly_checked(
                    c, opp, static_cast<int>(AbnormalStatusId::Petrify),
                    random_anomaly_duration(), owner, chance, ChanceSource::SoulMark);
            }
        });
    return EffectResult::kOk;
}

// ── 911：双方提升项数和 × db[0]；回合末解除自身能力下降 ──────────────────────
EffectResult k346_911_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !wire_once(ctx, kMark911, owner)) {
        return EffectResult::kOk;
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || !g_core || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark911)) {
                return;
            }
            const HostRow row = host_row(c, owner, kMark911);
            const int per = db_at(row, 0, 60);
            const int boosts = positive_level_count(c, owner)
                             + positive_level_count(c, c->opponent(owner));
            attach_fixed(c, owner, boosts * per);
        });
    return EffectResult::kOk;
}

EffectResult k346_911_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->clear_stat_drops) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark911)) {
        return EffectResult::kOk;
    }
    bool has_drop = false;
    for (int i = 0; i < kBattleStatSlots; ++i) {
        if (ctx->ability_levels[owner][i] < 0) {
            has_drop = true;
            break;
        }
    }
    if (has_drop) {
        (void)g_core->clear_stat_drops(ctx, owner);
    }
    return EffectResult::kOk;
}

// ── 946：回合首速度 +db[0]；速度正等级 × db[1] 附加固伤 ──────────────────────
EffectResult k346_946_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark946)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, owner, kMark946);
    const int delta = db_at(row, 0, 2);   // 在位即消费：显式 0 = 本行关闸
    if (delta > 0) {
        (void)g_core->stat_change(ctx, owner, kSpeedSlot, delta);
    }
    return EffectResult::kOk;
}

EffectResult k346_946_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !wire_once(ctx, kMark946, owner)) {
        return EffectResult::kOk;
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || !g_core || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark946)) {
                return;
            }
            const HostRow row = host_row(c, owner, kMark946);
            const int per = db_at(row, 1, 100);
            const int level = c->ability_levels[owner][kSpeedSlot];
            if (level > 0) {
                attach_fixed(c, owner, level * per);
            }
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3（877/911/946 入账，只增不改）──
SOUL_PROGRAM(kMark877,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k346_877_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true)});

SOUL_PROGRAM(kMark911,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k346_911_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k346_911_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});

SOUL_PROGRAM(kMark946,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k346_946_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &k346_946_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
