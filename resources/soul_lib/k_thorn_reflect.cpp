#include <algorithm>
// K3 反伤/反馈族 —— effect 11/183/328/330/386/1057（6 宏 / 10 宠 9 行）
//
// 行清单（tools/soul_effects_dump.py 11 183 328 330 386 1057，2026-10-01）：
//   11   比例反弹：2580 斗神瑞尔斯 1/6（无参）/ 2718 究影魔天 args[3,0]（db[0]=3 → 1/3）/
//        [1203,1204] 蒙塔迈尔斯 1/8（无参）——"每次受到攻击反馈给对手 1/N 的伤害"
//        → TAKE_DAMAGE（两攻击时点）watcher，反弹量 = 落血 ev.amount × 1/N（粉伤 FIXED）
//   328  定额反弹：2672 瞳神·耶里梅斯 args[60,0] "每次受到伤害反弹 60 点固定伤害"（db[0]=60）
//   1057 赤渊之蛇 args[250,70,1]：①"受到攻击伤害后反弹 250 点固定伤害"（db[0]=250）照做；
//        ②先出手闪避+70% ③后出手必暴 —— **按 K3 工单口径记档**（留待 P0 命中钩子；
//        ws.dodge_rate/ws.must_crit 槽在，回头补）
//   386  免疫+反弹异常 & 受伤双防：2800 帝龙圣尊 args[2,1]（db[0]=2 回合窗，db[1]=1 启用
//        受伤双防）/ 2913 玛努诺 args[0,1]（db[0]=0 = 无异常窗，只有双防子句）——
//        ①"首次上场前 {db0} 回合免疫并反弹异常（每场一次）"= 喵史 4872 同款
//        grant_immunity(ANOMALY, Modern) + rule_center_.grant_reflect；
//        ②"每次受到伤害时双防+1"（防御槽2 + 特防槽3 各 +1）
//   330  死亡继承：2674 星界祖皇 "死亡时下一只出战精灵继承自己的能力提升状态，
//        死亡时反弹自身所处的异常状态" → EVENT_DEATH 快照正等级 + 遍历异常弹给对手；
//        EVENT_ENTER_STAGE（本方）回填正等级（perform_switch ② 先清、⑥ 事件回填）
//   183  神·阿波罗 "反弹对手给予的能力下降" → **记档跳过**：引擎无"施加被免/被拦"拦截钩子，
//        只能在掉级落地后补弹（我方照掉、对手也掉 = 不是"反弹"）——需要内核微任务
//        （STAT_DROP 施加入口的 reflect 分流），不硬编。
//
// 反弹通用口径：落血 ev.amount/定额走 deal_pink_damage(FIXED)（犀牛 1217 同款 watcher
// 路线，非管线 DETECT 条目）；"受到(攻击)伤害"只认两攻击伤害时点；对手已倒地不弹。
// 三查记录（2026-10-01）：①内核无这 6 个魂印 id 硬编码；②effect_info 均技能空间模板；
// ③全库无同 id 注册（183/330/386/1057 首次落地）。
#include "common.h"

#include <primitives/battle_primitives.h>

// K7 补子句路由（定义在 k_spacetime_leap.cpp，全局作用域——跨 TU 引用）。
EffectResult soulmark_1057_leap_route(BattleContext* ctx, const EffectArgs& args);

namespace {

constexpr int kWiredKeyBase = 990681000;   // +宏*10+owner：watcher 已挂旗标（防 early 双触发）

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

bool is_attack_damage_event(const BattleEvent& ev) {
    return ev.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
        || ev.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
}

// 反弹出手方存活校验 + 粉伤定额/比例弹回（actor = 魂印方）。
void reflect_fixed(BattleContext* ctx, int owner, int foe, int amount) {
    if (amount <= 0 || !g_core || !g_core->deal_pink_damage) {
        return;
    }
    const ElfPet& foe_pet = ctx->getPet(foe);
    if (foe_pet.hp <= 0) {
        return;
    }
    g_core->deal_pink_damage(ctx, foe, amount, DamageKind::FIXED, owner);
}

// ── effect 11：受到攻击反馈 1/N 伤害 ────────────────────────────────────────

EffectResult soulmark_11_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    int slot = resolve_subject_slot_from_args(args);
    if (slot < 0) {
        slot = ctx->find_pet_with_soulmark(owner, 11);
    }
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const std::vector<int>& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    int denom = -1;
    if (!db.empty() && db[0] > 0) {
        denom = db[0];   // 2718 [3,0] → 1/3
    } else {
        const int monster = ctx->seerRobot[owner].elfPets[slot].soulMark.monster_id;
        if (monster == 2580) {
            denom = 6;
        } else if (monster == 1203 || monster == 1204) {
            denom = 8;
        }
    }
    if (denom <= 0) {
        return EffectResult::kOk;
    }
    if (watcher_wired(ctx, 11, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, denom](BattleContext* wctx, const BattleEvent& ev) {
            // ⚠️ ev.actor 可能是 -1（无行动方事件：效果类粉伤/反伤链等）——
            //   reflect_fixed 里 getPet(-1) 是 SeerRobot[2] 越界读（UBSan 抓现行，
            //   fuzz seed ~1601 段首曝），守卫必须带范围校验。
            if (!wctx || ev.target != owner || ev.actor < 0 || ev.actor > 1
                || ev.actor == owner || ev.amount <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, 11) || !is_attack_damage_event(ev)) {
                return;
            }
            reflect_fixed(wctx, owner, ev.actor, ev.amount / denom);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── effect 328 / 1057：受到伤害反弹定额固定伤害（db[0]）─────────────────────

EffectResult flat_reflect_arm_impl(BattleContext* ctx, const EffectArgs& args, int macro,
                                   int fallback_amount, bool attack_only) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    int slot = resolve_subject_slot_from_args(args);
    if (slot < 0) {
        slot = ctx->find_pet_with_soulmark(owner, macro);
    }
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const std::vector<int>& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    const int amount = (!db.empty() && db[0] > 0) ? db[0] : fallback_amount;
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    if (watcher_wired(ctx, macro, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, amount, macro, attack_only](BattleContext* wctx, const BattleEvent& ev) {
            // ⚠️ 同 effect 11：ev.actor 可能是 -1（无行动方事件），范围守卫必须带
            if (!wctx || ev.target != owner || ev.actor < 0 || ev.actor > 1
                || ev.actor == owner || ev.amount <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, macro)) {
                return;
            }
            // 328"每次受到伤害"= 任意伤害（粉/真都发 EVENT_TAKE_DAMAGE，时点不同）；
            // 1057/11 措辞带"攻击"→ 只认两攻击伤害时点。
            if (attack_only && !is_attack_damage_event(ev)) {
                return;
            }
            reflect_fixed(wctx, owner, ev.actor, amount);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

EffectResult soulmark_328_arm(BattleContext* ctx, const EffectArgs& args) {
    return flat_reflect_arm_impl(ctx, args, 328, 60, /*attack_only=*/false);
}

EffectResult soulmark_1057_arm(BattleContext* ctx, const EffectArgs& args) {
    return flat_reflect_arm_impl(ctx, args, 1057, 250, /*attack_only=*/true);   // ②③记档
}

// ── effect 386：首上场异常免疫+反弹窗（db[0] 回合，每场一次）+ 受伤双防+1（db[1]）──

EffectResult soulmark_386_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    int slot = resolve_subject_slot_from_args(args);
    if (slot < 0) {
        slot = ctx->find_pet_with_soulmark(owner, 386);
    }
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const std::vector<int>& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    const int anomaly_window = (!db.empty() && db[0] > 0) ? db[0] : 0;
    const bool guard_on_double_def = (db.size() >= 2) ? db[1] == 1 : true;

    // ② 受伤双防+1（watcher 每场一次；两行都启用）
    if (guard_on_double_def && !watcher_wired(ctx, 386, owner)) {
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.target != owner || ev.actor == owner || ev.amount <= 0) {
                    return;
                }
                if (!onstage_has_soul_mark(wctx, owner, 386) || !is_attack_damage_event(ev)) {
                    return;
                }
                if (g_core && g_core->stat_change) {
                    g_core->stat_change(wctx, owner, /*防御槽=*/2, 1);
                    g_core->stat_change(wctx, owner, /*特防槽=*/3, 1);
                }
            },
            WatcherScope::TEAM);
    }

    // ① 首上场窗（每场战斗一次；早期节点双触发由 plugin_storage 幂等挡住）
    if (anomaly_window <= 0) {
        return EffectResult::kOk;   // 玛努诺 [0,1]：无异常窗，只有双防子句
    }
    constexpr int kFirstEntranceUsedKey = 990682000 + 386 * 10;   // +owner
    const int used_key = kFirstEntranceUsedKey + owner;
    if (ctx->plugin_storage.count(used_key)) {
        return EffectResult::kOk;   // "每场战斗只能触发一次"
    }
    ctx->plugin_storage.emplace(used_key, 1);
    // 喵史 4872 ①同款：Modern 层异常免疫 + REFLECT 票（主动毒不可见，见 rule_center 层级口径）。
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/anomaly_window, /*source_id=*/386 * 10 + 1,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/386, /*tier=*/ImmunityTier::Modern);
    ctx->rule_center_.grant_reflect(owner, /*source_slot=*/-1, /*source_effect_id=*/386,
                                    /*target=*/owner, /*rounds=*/anomaly_window);
    return EffectResult::kOk;
}

// ── effect 330：死亡继承正等级 + 反弹自身异常 ───────────────────────────────

constexpr int kInheritBoostsKeyBase = 990683000;   // +owner：死亡快照（[s0..s5] 六槽）

bool host_died(BattleContext* ctx, int side, int slot, int mark_id) {
    if (slot < 0 || slot > 5) {
        return false;
    }
    return ctx->seerRobot[side].elfPets[slot].soulMark.id == mark_id;
}

EffectResult soulmark_330_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (watcher_wired(ctx, 330, owner)) {
        return EffectResult::kOk;
    }
    // 死亡时点：快照正等级 + 异常反弹给对手（EVENT_DEATH 时切宠未发生、等级还在）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || !host_died(wctx, owner, ev.slot, 330)) {
                return;
            }
            // ① 快照能力提升（正等级；降级不继承——文本只说"提升状态"）。
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                const int level = wctx->ability_levels[owner][i];
                wctx->plugin_storage[kInheritBoostsKeyBase + owner * 10 + i] = std::max(0, level);
            }
            // ② 反弹自身所处异常给对手（Modern 通道全链；时长用默认施加档）。
            //    在效判定直读 abnormal_status_end_round（内核符号插件不可链，K1 同款）。
            if (g_core && g_core->apply_anomaly_checked) {
                for (int id = 0; id <= static_cast<int>(AbnormalStatusId::SlowFreeze); ++id) {
                    if (wctx->roundCount < wctx->abnormal_status_end_round[owner][id]) {
                        g_core->apply_anomaly_checked(wctx, 1 - owner, id,
                                                      random_anomaly_duration(), owner,
                                                      /*chance_pct=*/-1, ChanceSource::SoulMark);
                    }
                }
            }
        },
        WatcherScope::TEAM);
    // 下一只出战精灵：回填正等级（perform_switch ② 清等级 → ⑥ 事件时回填生效）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner) {
                return;
            }
            const auto snapshot = wctx->plugin_storage.find(kInheritBoostsKeyBase + owner * 10);
            if (snapshot == wctx->plugin_storage.end()) {
                return;   // 没有星界祖皇死亡过 → 无继承
            }
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                const int level = any_int_or(
                    wctx->plugin_storage[kInheritBoostsKeyBase + owner * 10 + i]);
                if (level > 0) {
                    wctx->ability_levels[owner][i] = std::min(level, 6);   // 官方等级域钳顶
                }
            }
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                wctx->plugin_storage.erase(kInheritBoostsKeyBase + owner * 10 + i);   // 一次性
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +5（183 记档跳过，只增不改）──
SOUL_PROGRAM(11, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_11_arm,
                   /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(328, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_328_arm,
                    /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(330, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_330_arm,
                    /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(386, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_386_arm,
                    /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(1057, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1057_arm,
                     /*once=*/false, /*early=*/true),
                    SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &soulmark_1057_leap_route,
                     /*once=*/false, /*early=*/false)});
