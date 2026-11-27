// K372 暴击概率+未触发补偿+吸血族 —— effect 1227（1 宏 / 2 行）
//（异星装甲·赛伯斯特 3652 row937 + row1475 伙伴行；号段 7170-7179，
//  worktree ../agents/k372，2026-10-24）
//
// 行清单（tools/soul_effects_dump.py 1227，2026-10-24）：
//   row937 args[50,1,100,50,200,4,1] kind[0,1,5]：
//     「自身攻击技能有50%的概率打出致命一击，未触发则下回合所有技能先制+1且
//       100%的概率打出致命一击（boss有效）；造成伤害的50%恢复自身体力，
//       若恢复的体力低于200则吸取对手最大体力的1/4（boss无效）」
//   row1475 args[70,1,100,50,200,3,1]（X之战意伙伴效果）：同文 50→70、4→3。
//
// ⚠️ 三查记录（author 立单 2026-10-24 / a76-jprod 动工同日复核）：
//   ①内核 grep \b1227\b 无魂印行为硬编码；②effect_info 1227 技能空间模板异文
//   （tips 权威）；③registry soul 无 1227、soul_lib 全库 grep 无同 id 注册——真缺口。
//
// 行参解码（两行同构、5/3 随行变强锚自证）：
//   db[0]=暴击申报概率%（50/70）、db[1]=1 旗（不消费——K1 纪律）
//   db[2]=补偿暴击概率%（两行恒 100，roll_percent ≥100 短路不耗 rand）
//   db[3]=吸血%（50）、db[4]=低额阈值（200）、db[5]=吸取分母（4/3）
//   db[6]=1 旗（不消费——boss 口径按 tips 分句落注册面，见下）
//
// 实现（板行处方）：
//   ① 暴击申报（boss有效 → 节点 boss_invalid=false）：FIRST/SECOND_BEFORE_SKILL_HIT
//     桶（k300 1223 消费位同款；命中时点消费 skills.cpp:948）——roundChoice 攻击技门
//     → 补偿态（回合戳==roundCount-1）用 db[2]、否则用 db[0] 掷 → ws.must_crit[owner]
//     =true；掷败（非补偿态）记回合戳（=本攻击未暴击即"未触发"，miss 同判，记档）。
//     补偿态 100% 档短路不耗 rand（SOP §五.4）→ 补偿回合不续戳，窗恰一回合。
//   ①' 下回合先制：FMR 桶戳消费 → ws.preemptive_level[owner] += 1（K363/K218 同款，
//     memset 后写；roundCount 基准见 k300 记档：ROUND_START(N) 时 roundCount==N-1）。
//   ② 吸血（boss无效 → arm 节点 boss_invalid=true）：DETECT 相位伤害效果
//     （k189 register_default_damage_effect 同位，watcher_wired 幂等防 early 双挂）——
//     自己的红伤 final>0 → heal = final×db[3]/100 → heal_amount；
//     heal < db[4] → 吸取对手 max_hp/db[5]（K338 拆原语：deal_pink_damage(FIXED)
//     + heal_amount 等量，不越过对手存量）。阈值比较用**公式量**（final×%截断），
//     非封回血后落地量（被封回血面 plain 对局等价，记档）。
// 注册：SOUL_PROGRAM(1227) 四节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_7170_crit_comp_drain.cpp（7170-7179）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1227;
constexpr int kCritFallback = 50;      // db[0]（tips 字面 50%）
constexpr int kCompCritFallback = 100; // db[2]（两行恒 100）
constexpr int kHealPctFallback = 50;   // db[3]（tips 字面 50%）
constexpr int kHealFloorFallback = 200;// db[4]（tips 字面 200）
constexpr int kDrainDivFallback = 4;   // db[5]（tips 字面 1/4）

// ── 跨回合补偿旗标（soulmark_storage 容器，K218/K363 同款）──────────────────
struct K372Runtime {
    int crit_fail_round = -1;   // 最近一次攻击技暴击掷败的回合
};

K372Runtime& k372_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K372Runtime{};
    }
    return std::any_cast<K372Runtime&>(storage[kMarkId]);
}

// ── 宿主行参数（db_at 原序读 owned_int_args——K279/K363 同款）──────────────────
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

// 所选技是不是攻击技（roundChoice 读法，k300 同款）。
bool used_attack_skill(BattleContext* ctx, int owner) {
    if (!ctx || ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return false;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5) {
        return false;
    }
    return ctx->getPet(owner).skills[idx].type != SkillType::Attribute;
}

// ── ① BEFORE_SKILL_HIT：暴击申报（正常 db[0] / 补偿态 db[2]）─────────────────
EffectResult k372_1227_before_skill_hit(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    if (!used_attack_skill(ctx, owner)) {
        return EffectResult::kOk;   // 「自身攻击技能」门（属性技不可暴，天然无申报面）
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    K372Runtime& rt = k372_runtime(ctx, owner);
    const bool comp_active =
        rt.crit_fail_round >= 0 && rt.crit_fail_round == ctx->roundCount - 1;
    const int pct = comp_active ? db_at(row, 2, kCompCritFallback)
                                : db_at(row, 0, kCritFallback);
    if (roll_soul_percent(*ctx, kMarkId, 0, owner, owner, pct)) {
        ctx->ws.must_crit[owner] = true;   // 命中时点消费（skills.cpp:948）
    } else if (!comp_active) {
        rt.crit_fail_round = ctx->roundCount;   // 未触发 → 下回合补偿
    }
    return EffectResult::kOk;
}

// ── ①' FMR：下回合所有技能先制 +1（戳消费）───────────────────────────────────
EffectResult k372_1227_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const K372Runtime& rt = k372_runtime(ctx, owner);
    if (rt.crit_fail_round < 0 || rt.crit_fail_round != ctx->roundCount - 1) {
        return EffectResult::kOk;   // 哨兵 -1 首回合防撞（k300 roundCount 基准记档）
    }
    ctx->ws.preemptive_level[owner] += 1;
    return EffectResult::kOk;
}

// ── ② DETECT：吸血 + 低额吸取（arm 幂等，boss_invalid 走 arm 节点）────────────
constexpr int kWiredKeyBase = 990372000;   // +owner：DETECT 效果已挂旗标

EffectResult k372_1227_arm_detect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // early 双触发防重（k189 同款）
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->heal_amount ||
                !g_core->deal_pink_damage) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 只随自己的攻击红伤吸血
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const int opp = 1 - owner;
            // 行参（事件面无 SoulNodeArgs——k189 同款 find 宿主读 owned_int_args）。
            int heal_pct = kHealPctFallback;
            int heal_floor = kHealFloorFallback;
            int drain_div = kDrainDivFallback;
            const int slot = c->find_pet_with_soulmark(owner, kMarkId);
            if (slot >= 0 && slot <= 5) {
                const auto& db =
                    c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
                if (db.size() > 3) {
                    heal_pct = db[3];
                }
                if (db.size() > 4) {
                    heal_floor = db[4];
                }
                if (db.size() > 5 && db[5] > 0) {
                    drain_div = db[5];
                }
            }
            if (heal_pct <= 0) {
                return;
            }
            const int heal = d.final * heal_pct / 100;   // 公式量（记档：非封回血落地量）
            if (heal > 0) {
                (void)g_core->heal_amount(c, owner, heal);
            }
            if (heal >= heal_floor) {
                return;   // 恢复达标 → 无吸取
            }
            // 吸取对手最大体力的 1/db[5]（K338 拆原语：粉伤 FIXED + 等量回复，
            // 不越过对手存量；deep pink 不走红伤管线 → d.isRed 门天然防递归）。
            if (drain_div <= 0) {
                return;
            }
            const int opp_max = resolve_pet_max_hp(c->getPet(opp));
            int drain = opp_max / drain_div;
            const int foe_hp = c->getPet(opp).hp;
            if (drain > 0 && foe_hp > 0) {
                if (drain > foe_hp) {
                    drain = foe_hp;   // 吸取不越过存量（K302/K338 口径）
                }
                (void)g_core->deal_pink_damage(c, opp, drain, DamageKind::FIXED, owner);
                (void)g_core->heal_amount(c, owner, drain);
            }
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1227 入账，只增不改）──────
// boss 口径分句（tips）：①暴击/补偿句（boss有效）→ 三节点 boss_invalid=false；
// ②吸血句（boss无效）→ arm 节点 boss_invalid=true（boss 挑战不挂 DETECT 效果）。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k372_1227_arm_detect,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_BEFORE_SKILL_HIT, &k372_1227_before_skill_hit,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_SECOND_BEFORE_SKILL_HIT, &k372_1227_before_skill_hit,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k372_1227_fmr,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
