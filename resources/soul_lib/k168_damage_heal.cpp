// K168 伤害回血族 —— effect 363（1 宏 / 2 行）
//
// 行清单（tools/soul_effects_dump.py 363，2026-10-06）：
//   363 阿米尔 2757 args[30, 0] / 悲鸣神护 3059 args[40, 0]（kind[5]）：
//     "所有攻击技能造成的伤害的30%/40%恢复自身体力"
//     → db[0]=回血百分比。ATTACK... DETECT 相位零状态票（attacker==持有方、isRed、
//     final>0）→ heal_amount(final×pct/100)（吃封回血；DETECT 时点 final 已定、
//     hp 未扣——J 侧 1415/363 同族 moves 实现同相位）。
//
// ⚠️ 同板 266（闪避压制）已由在途 K164（k_attack_prob.cpp）覆盖——本单**跳过 266**
//    只做 363，避免双实现（看板行含 266 系 author 并集竞态，对账以本记档）。
//
// ⚠️ 三查记录（2026-10-06）：①内核无 363 魂印行为硬编码 ②effect_info 无 363 技能
//    模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：yanshou（register_default_damage_effect 双侧接线 + 实时读）、
//   K12 762（零状态票）、J 侧 1415/363 同族（DETECT 相位 heal）。
// 场景回归：test/scenario/scenario_3020_damage_heal.cpp
//（⚠️ 号段记档：看板行 2730-2739 与 J171 场景段撞号（author 分配竞态），本单场景
//   改用 alloc 实配 3020-3029；看板行不回改，以本记档对账。）

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kWiredBase = 990168000;   // +宏*10+owner：管线已接线旗

// 宿主行解析（K2 host_row 私副本）。
struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const EffectArgs& args, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = resolve_subject_slot_from_args(args);
    if (slot < 0) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.monster_id = ctx->seerRobot[owner].elfPets[slot].soulMark.monster_id;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const auto key = static_cast<unsigned long long>(kWiredBase + macro * 10 + owner);
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── 363 enter 节点：接线 DETECT 零状态票（攻击伤害 pct% 回血）──
EffectResult k168_363_enter(BattleContext* ctx, const EffectArgs& args) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 363, owner)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 363);
    const int pct = db_at(row, 0, 30);
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, pct](BattleContext* c, int bucket_owner) {
            if (bucket_owner != owner) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (g_core && g_core->heal_amount) {
                (void)g_core->heal_amount(c, owner, d.final * pct / 100);
            }
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(363, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k168_363_enter,
                    /*once=*/false, /*early=*/true)});
