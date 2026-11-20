#include <utils/sbs_rand.h>
// K373 异常门降属+已损恢复+动态概率威力翻倍族 —— effect 856（1 宏 2 行）
//
// 行清单（tools/soul_effects_dump.py 856，2026-10-24）：
//   856 row567（3242 王·盖亚）args[2,1,50,30,1]：
//     「回合开始时若自身处于异常状态则对手2项属性-1且造成的伤害减少50%；每回合
//      恢复自身已损失体力的30%，攻击时有自身已损失体力百分比的概率威力翻倍（BOSS无效）」
//   856 row568（3242 战斗传承进化行）args[4,1,60,35,1]：同构互锚 4 项/60%/35% 档。
//
// 行参解码（两行互锚自证；⚠️ 行≠模板：effect_info 856 = 技能空间同号模板
//   「消除对手回合类效果，消除成功则恢复自身最大体力的1/{0}」args_num=1，异文，
//   tips 权威）：
//   db[0]=降属项数（2/4）    db[1]=降幅（1）
//   db[2]=对手输出减伤%（50/60）  db[3]=已损恢复%（30/35）
//   db[4]=旗恒 1（两行同值，语义不可辨——按 K2 纪律不消费，记档）
//
// 落法（板行处方逐子句）：
//   ① 异常门（"回合开始时若自身处于异常状态"）：BATTLE_ROUND_START 采样
//      has_any_active_abnormal_status(owner)（官方异常槽遍历，447 套装线同款判定式）
//      → 置当回合门旗（K361 819 同款每回合无脑重写，官方条件采样在回合开始时点）。
//   ② 门开 → 对手随机 N 项 -1：k893 rand%6 去重循环 → stat_drop（免弱/钳制原语内查；
//      AT_FLOOR 槽照常计入项数）。
//   ③ 门开 → 对手输出减伤：REDUCE_PCT/MITIGATE 旗消费票（K5 725 直系；isRed only，
//      final×(100-db[2])/100）。⚠️ 板行处方"DETECT 票"按子句直系落 REDUCE_PCT 相位
//      （725"受伤-X%"同形；725/K203 混称记档）——减伤不可被挡伤失效无视。
//   ④「每回合恢复自身已损失体力的30%」：分号独立子句（"每回合"不带异常门，两行
//      行文一致）——同 ROUND_START 桶无条件 heal=(max-hp)×db[3]/100（K281 610 形
//      换已损基数；heal_amount 恢复原语）。
//   ⑤「攻击时有自身已损失体力百分比的概率威力翻倍」：K220 292 直系+概率动态化——
//      FIRST/SECOND_SKILL_EFFECT 时点（SOP §二.3 威力改写时点）roundChoice+类型表门
//      → pct=(max-hp)×100/max 本地掷点（威力翻倍非异常通道，允许本地 roll）→
//      skill_power_view ×2。
//   boss_invalid：tips 尾注"（BOSS无效）"覆盖全部分句 → 全节点统一置位（漏标=半初始化）。
//   行参读取：db_at 读宿主 owned_int_args 原序（方案 B，K257 起夜跑口径）。
//
// 三查记录（2026-10-24，worktree ../agents/k373）：
//   ①内核 grep 856 无魂印行为硬编码；②effect_icon kind=[3,5,12]、tips 为权威文
//   （dump 原文已核，见上）；③registry soul 集合无 856、soul_lib 全库 grep 无
//   SOUL_PROGRAM(856)——真缺口。
// 先例：k253 336（ROUND_START 持续态门判据）、k893（随机去重 stat_drop 循环）、
//   k_hit_dodge_aura 725（REDUCE_PCT/MITIGATE 减免票）、k361 819（回合门旗无脑重写）、
//   k281 610（ROUND_START heal+resolve_pet_max_hp）、k220 292（SKILL_EFFECT 掷点
//   威力翻倍全形态）。
// 场景回归：test/scenario/scenario_7101_k373_anomaly_gate.cpp（号段 7101）。

#include "common.h"

#include <entities/skills.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 856;
constexpr int kWiredBase = 990373000;   // 接线去重：+宏*10+owner（grep 无撞）
constexpr int kGateFlagBase = 990373500;  // 异常门当回合旗：+owner

// ── 宿主行参数（方案 B：db_at 读 owned_int_args 原序，与真实对局一致）────────
struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
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
    const ElfPet& host = ctx->seerRobot[owner].elfPets[slot];
    row.db = &host.soulMark.args.owned_int_args;
    row.monster_id = host.soulMark.monster_id;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

bool watcher_wired(BattleContext* ctx, int owner) {
    const int key = kWiredBase + kMarkId * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── arm（enter/early）：挂对手输出减伤票（门旗在 ROUND_START 每回合重写）─────
EffectResult k856_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || watcher_wired(ctx, owner)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int reduce_pct = db_at(row, 2, 0);
    if (reduce_pct <= 0) {
        return EffectResult::kOk;
    }
    ctx->register_default_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner, reduce_pct](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (bucket_owner != owner || d.defenderId != bucket_owner || !d.isRed
                || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            auto it = c->plugin_storage.find(kGateFlagBase + owner);
            if (it == c->plugin_storage.end() || any_int_or(it->second, 0) != 1) {
                return;   // 本回合回合开始时自身无异常 → 门闭
            }
            d.final = d.final * (100 - reduce_pct) / 100;
        });
    return EffectResult::kOk;
}

// ── ROUND_START：已损恢复（无条件子句）+ 异常门（旗重写 + 随机 N 项降属）─────
EffectResult k856_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int drop_count = db_at(row, 0, 0);
    const int drop_amount = db_at(row, 1, 0);
    const int heal_pct = db_at(row, 3, 0);
    const int opp = 1 - owner;

    // ④ 每回合恢复已损体力（分号独立子句，无异常门）
    if (heal_pct > 0 && g_core->heal_amount) {
        ElfPet& self = ctx->getPet(owner);
        if (self.hp > 0) {
            const int max_hp = resolve_pet_max_hp(self);
            const int heal = (max_hp - self.hp) * heal_pct / 100;
            if (heal > 0) {
                (void)g_core->heal_amount(ctx, owner, heal);
            }
        }
    }

    // ① 异常门采样（回合开始时点，K361 同款每回合无脑重写）
    const bool gate_on = ctx->has_any_active_abnormal_status(owner);
    ctx->plugin_storage[kGateFlagBase + owner] = gate_on ? 1 : 0;

    // ② 门开 → 对手随机 N 项 -1（k893 去重循环）
    if (gate_on && drop_count > 0 && drop_amount > 0 && g_core->stat_drop) {
        int picked[6] = {};
        int done = 0;
        int guard = 0;
        while (done < drop_count && guard < 100) {
            ++guard;
            const int slot = sbs_rand() % 6;
            if (picked[slot]) {
                continue;
            }
            picked[slot] = 1;
            ++done;
            (void)g_core->stat_drop(ctx, opp, slot, drop_amount, owner);
        }
    }
    return EffectResult::kOk;
}

// ── ⑤ SKILL_EFFECT：动态概率威力翻倍（K220 直系+概率动态化）─────────────────
EffectResult k856_roll_power(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5
        || ctx->getPet(owner).skills[idx].type == SkillType::Attribute) {
        return EffectResult::kOk;   // 只翻攻击技（K4 判据：类型表门）
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    const int max_hp = resolve_pet_max_hp(self);
    const int pct = (max_hp - self.hp) * 100 / max_hp;   // 已损失体力百分比
    if (pct <= 0 || !roll_percent(pct)) {
        return EffectResult::kOk;   // 满血不翻；本地掷点（威力翻倍非异常通道）
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * 2;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（856 入账，只增不改）──────
SOUL_PROGRAM(kMarkId, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k856_arm,
                             /*once=*/false, /*early=*/true, SoulScope::STAGE,
                             /*boss_invalid=*/true),
                       SOUL_NODE(State::BATTLE_ROUND_START, &k856_round_start,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/true),
                       SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k856_roll_power,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/true),
                       SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k856_roll_power,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/true)});
