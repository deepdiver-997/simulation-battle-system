#include <utils/sbs_rand.h>
// 王之哈莫 3022（魂印 effect_icon 715）+ 重生之翼 2987（魂印 effect_icon 866）
// —— 锚点窗口式魂免族（2026-09-23，调研文档：
//    docs_local/docs/06-交接/重生之翼与王之哈莫-魂免窗口与下场保留-调研.md）
//
// ═══ 官方口径（语料三篇 + DB effect_icon tips）═══
// 【王之哈莫 715】（args: 75 10 15 3 20 20）
//   [减伤] 满体力受伤-75%，每降10%体力减伤-15%（**乘法**，语料《机制解析—王之哈莫》）。
//   [触发] 描述"每回合开始时若体力高于对手则免异常"是简化文本，实测判定：
//         每回合开始时判体力 → 若高于对手，**任意先手方技能命中前**开启免控，
//         免控延续至**任意先手方下1次技能命中前（下场保留）**——窗口跨回合边界，
//         上一窗口到期点 = 下一窗口武装点（战龙哈莫同族"转换式魂免"的单判定点版）。
//         若低于对手且任意一方技能命中前 → 回合结束后（扣减点之后）回 1/3；
//         体力为 0 不触发；双方均不出手则两个分支都拿不到（时点桶结构性保证：
//         命中前锚点不发、28 时点在回合计数后仍会跑——低体回血靠"命中前锚点确认位"
//         门控，本轮无技能 → 无确认 → 不回血）。
//         ⚠️ 语料未列王哈的"游戏开始首免"（战龙哈莫有）——按没有落地。
//   [限定] 最大体力 +20%（战前补丁 STAT_MODIFY）、受伤 -20%（并入减伤管线乘法）。
//
// 【重生之翼 866】（args: 100 1 8 8 3 1 60 3）
//   ① 被击败后 100% 随机复活出战背包内已被击败的 1 只精灵（无法复活自己；
//     被消逝的精灵除外——revive_pet 原语原生拒绝；复活体力 = 游戏开始时记录值，
//     官方 idx=224 后场复活口径）。
//   ② 神耀能量 ≤6 层（层变动只经 hamo_wing_runtime.h 的 wing_adjust_layers，
//     ≥2 的免控/免弱票在层越线时授/撤）：
//     ≥1 受到攻击伤害 -8%/层（乘法；4399 文本"满层(5)就是40%减伤"= 线性 8×层数）；
//     ≥2 免疫所有异常 + 免疫所有能力下降——语料"开启后**闭环免控**直到层数<2"
//        （coverage_all + Ancient 层：两只在《浅谈主动毒与魂印免控判定》老魂免名单）；
//     ≥3 自身出手起每回合结束回 1/3；
//     ≥4 立即免疫并**记录**受到的固定伤害，回合一端（ROUND_END 双侧桶）反弹等量固伤；
//        记录**下场保留**；晚于本侧反弹点的固伤自然落到下回合；
//     ≥5 攻击技能先制 +1；
//     ≥6 攻击技能伤害 +60%（**加法增伤**——语料"6层的增伤60%是加法增伤"）；
//        第五技能威力提升不消耗层数（862 消耗效果按槽位豁免，见 wing_skills.cpp）。
//   ③ 赛尔对战击败对手且层数≥6 → 挂起 bundle：神耀能量在"**下回合先手方命中前**"
//     自动清 0，且**所有技能先制+3**；清零与先制**均下场保留**（语料实测：击杀当回合
//     立马下场，下次登场仍持有 bundle，且清零前 ≥2 层闭环免控还在——可免出战击杀控/
//     登场控/回合开始控/部分命中前控）。
//     落地口径：MOVE_RIGHT 时点若在场 → +3 并记"激活回合"；命中前锚点若挂起且
//     激活回合==本回合 → 清零。在场未下场 = 击杀次回合；下场再登场 = 重登次回合
//     （MOVE_RIGHT 时不在场不加+3也不清零 → 重登当回合的回合开始控/命中前控
//     都被残留层数的闭环免控罩住，与语料列表一致）。非赛尔对战 4 回合变体不建模
//     （引擎无对战模式位，见调研文档 §四）。
//
// ═══ 实现要点 ═══
//   · "任意先手方技能命中前"是**全局锚点**：动作流程桶只跑行动方 → 走内核新增的
//     EVENT_BEFORE_SKILL_HIT（先手方/后手方每回合去重，emit 后立即 drain——
//     监听方授票必须先于该时点桶，才能挡住桶内的命中前控）。
//   · 击杀归因 = EVENT_DEATH 的 ev.actor == 己方 && ev.target == 对方（2260/071 契约）。
//   · 状态全部走 hamo_wing_runtime.h 的 **int 发布槽**（soul/moves/测试三方共享，
//     跨 dylib any_cast 只对 fundamental type 安全）。
//   · 减伤走伤害管线 REDUCE_TRAIT 阶段（乘法、早于减伤区），回调实时读体力/层数；
//     ON_STAGE 条目随切换作废、arm 节点以 pipeline_valid_id 防重注册。
#include "common.h"

#include "../hamo_wing_runtime.h"

namespace {

// 免疫票固定句柄（grant source_id>0 = 复用更新；全局唯一命名空间）
constexpr int kHamoHandleWindow = 7150091;    // 王哈：锚点窗口（免异常，魂免/Ancient）
constexpr int kWingHandleAnomaly = 8660091;   // 重生 ≥2：免异常（闭环）
constexpr int kWingHandleStatDrop = 8660092;  // 重生 ≥2：免能力下降（闭环）

constexpr int kWingMarkId = 866;
constexpr int kHamoMarkId = 715;

int pet_int(const ElfPet& pet, int key, int def = 0) {
    return hamo_wing_get(pet.soulmark_storage, key, def);
}
void set_pet_int(ElfPet& pet, int key, int value) {
    hamo_wing_set(pet.soulmark_storage, key, value);
}

// ════════════════════════════════════════════════════════════════════
// 王之哈莫 715
// ════════════════════════════════════════════════════════════════════

// ── 节点 H1：BATTLE_ROUND_START（STAGE）—— 触发效果的回合开始判定 ──
// 高体/低体只判一次（本回合内生效，键带回合号防跨回合误用）。判定要求在场
// （STAGE 节点天然满足——"触发效果需要王哈在场时"，语料实测第 2 条）。
EffectResult hamo_715_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->getPet(owner);
    if (host.hp <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& opp = ctx->getPet(1 - owner);
    const bool high = host.hp > opp.hp;
    const bool low = host.hp < opp.hp;
    set_pet_int(host, kHamoJudgeRoundKey, ctx->roundCount);
    set_pet_int(host, kHamoHighKey, high ? 1 : 0);
    set_pet_int(host, kHamoLowKey, low ? 1 : 0);
    return EffectResult::kOk;
}

// ── 节点 H2：BATTLE_ROUND_REDUCTION_NEW_ROUND_END（STAGE）—— 低体回血 ──
// 语料：低体回血在"回合结束后（**扣减点之后**）"；须"任意一方技能命中前"触发过
// （命中前锚点写确认位）；自身体力为 0 不触发；对局先结束则到不了这里（FINISHED
// 不跑桶），与语料"击杀对方最后一只不触发回血"吻合。
EffectResult hamo_715_low_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->getPet(owner);
    if (host.hp <= 0) {
        return EffectResult::kOk;
    }
    if (pet_int(host, kHamoLowConfirmedRoundKey, -1) != ctx->roundCount) {
        return EffectResult::kOk;   // 本轮没有"命中前"确认（双方均不出手等）
    }
    const int amount = resolve_pet_max_hp(host) / 3;
    if (amount > 0) {
        g_core->heal_amount(ctx, owner, amount);
    }
    return EffectResult::kOk;
}

// ── 减伤管线（H3）：满体力 75% 每降 10% -15% + 限定 20% —— 乘法连乘 ──
// REDUCE_TRAIT 阶段（乘法、早于通用减伤区——语料"与绝大部分减伤效果乘法计算"）。
// 实时读当前体力比例（本回合内被打后减伤跟着降，不是回合开始快照）。
// ON_STAGE 条目随切换作废；arm 节点用 pipeline_valid_id 防重复注册。
EffectResult hamo_715_arm(BattleContext* ctx, const EffectArgs& args);

void hamo_715_register_pipeline(BattleContext* ctx, int owner, ElfPet& host) {
    const int current = ctx->pipeline_valid_id[owner];
    if (pet_int(host, kHamoPipelineValidKey, -1) == current) {
        return;   // 本登场周期已注册（防 arm 重跑叠条目）
    }
    set_pet_int(host, kHamoPipelineValidKey, current);
    ctx->register_damage_effect(
        DamagePhase::REDUCE_TRAIT, owner, DamageEffectCategory::MITIGATE,
        [](BattleContext* wctx, int bucket_owner) {
            DamageSnapshot& d = wctx->resolvedDamage;
            if (d.defenderId != bucket_owner || d.final <= 0) {
                return;
            }
            ElfPet& self = wctx->getPet(bucket_owner);
            if (self.hp <= 0 || self.soulMark.id != kHamoMarkId) {
                return;   // 在场受伤的不是王哈
            }
            const int max_hp = resolve_pet_max_hp(self);
            // 满体力 75%，每降 10% 体力减伤 -15%（向下取整到 10% 档；钳非负）
            const int missing_tenths = std::max(0, (max_hp - self.hp) * 10 / std::max(1, max_hp));
            int pct = 75 - 15 * missing_tenths;
            if (pct < 0) {
                pct = 0;
            }
            d.final = d.final * (100 - pct) / 100;   // 减伤主项
            d.final = d.final * 80 / 100;            // 限定：受伤 -20%（与主项乘法）
        },
        EffectScope::ON_STAGE);
}

// ── arm 总入口（ENTER_EXIT_STAGE，ROSTER）—— watcher 注册 + 管线注册 ──
EffectResult hamo_715_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int host_slot = ctx->find_mark_in_roster(owner, kHamoMarkId);
    if (host_slot < 0) {
        return EffectResult::kOk;   // 已消逝
    }
    ElfPet& host = ctx->seerRobot[owner].elfPets[host_slot];
    hamo_715_register_pipeline(ctx, owner, host);

    // watcher 只挂一次（帝君同款 flag；soulmark_storage 随 clearAllEffects 清 → 新对局重挂）
    if (hamo_wing_get(host.soulmark_storage, kHamoWatchersKey) == 0) {
        hamo_wing_set(host.soulmark_storage, kHamoWatchersKey, 1);

        // ── 命中前全局锚点：撤旧窗 → 条件成立则授新窗 / 低体确认 ──
        // 窗口 = [本轮命中前, 下轮命中前)：本轮锚点先撤上一窗口（到期），
        // 再按"本轮回合开始判定"决定是否重授——判定链断了的回合窗口自然开到锚点为止。
        // 下场保留：票 TEAM 作用域 + 窗口在本宠离场后仍存续到到期锚点。
        ctx->event_center_.register_watcher(
            EventType::EVENT_BEFORE_SKILL_HIT, owner, ctx->roundCount, /*duration=*/0,
            /*once=*/false,
            [owner](BattleContext* wctx, const BattleEvent& ev) {
                (void)ev;
                if (!wctx) {
                    return;
                }
                const int slot = wctx->find_mark_in_roster(owner, kHamoMarkId);
                if (slot < 0) {
                    return;   // 已消逝：无窗口
                }
                ElfPet& host = wctx->seerRobot[owner].elfPets[slot];
                wctx->revoke_immunity(owner, kHamoHandleWindow);   // 上一窗口到期
                if (pet_int(host, kHamoJudgeRoundKey, -1) != wctx->roundCount) {
                    return;   // 本轮没有在场判定（场下/刚登场未判）→ 只撤不授
                }
                if (pet_int(host, kHamoHighKey) == 1) {
                    // 免控窗口：魂免（抗性后才查）+ Ancient 层（老魂免名单，挡主动毒）
                    wctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                                         /*anomaly_mask=*/0, /*duration_rounds=*/0,
                                         kHamoHandleWindow, /*soul=*/true,
                                         EffectScope::TEAM, /*counts=*/0, kHamoMarkId,
                                         ImmunityTier::Ancient);
                }
                if (pet_int(host, kHamoLowKey) == 1) {
                    set_pet_int(host, kHamoLowConfirmedRoundKey, wctx->roundCount);
                }
            },
            WatcherScope::TEAM);

        // ── 消逝 → 窗口撤销（帝君御撤票同款）──
        ctx->event_center_.register_watcher(
            EventType::EVENT_VANISH, owner, ctx->roundCount, /*duration=*/0,
            /*once=*/false,
            [owner](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.target != owner) {
                    return;
                }
                if (wctx->is_vanished(owner, ev.slot)
                    && wctx->seerRobot[owner].elfPets[ev.slot].soulMark.id == kHamoMarkId) {
                    wctx->revoke_immunity(owner, kHamoHandleWindow);
                }
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 重生之翼 866
// ════════════════════════════════════════════════════════════════════

// ── 节点 W1：ENTER_EXIT_STAGE（ROSTER）—— arm：复活体力记录 + watcher + 管线 ──
EffectResult wing_866_arm(BattleContext* ctx, const EffectArgs& args);

void wing_866_register_pipeline(BattleContext* ctx, int owner, ElfPet& host) {
    const int current = ctx->pipeline_valid_id[owner];
    if (pet_int(host, kWingPipelineValidKey, -1) == current) {
        return;
    }
    set_pet_int(host, kWingPipelineValidKey, current);
    // ≥1：受到攻击伤害 -8%/层（乘法；实时读层数——4399 文本满 5 层 40% = 线性 8×层数）
    ctx->register_damage_effect(
        DamagePhase::REDUCE_TRAIT, owner, DamageEffectCategory::MITIGATE,
        [](BattleContext* wctx, int bucket_owner) {
            DamageSnapshot& d = wctx->resolvedDamage;
            if (d.defenderId != bucket_owner || d.final <= 0) {
                return;
            }
            ElfPet& self = wctx->getPet(bucket_owner);
            const int layers = wing_layers(self.soulmark_storage);
            if (layers <= 0) {
                return;
            }
            const int pct = 8 * layers;
            d.final = d.final * (100 - pct) / 100;
        },
        EffectScope::ON_STAGE);
    // ≥4：立即免疫并记录受到的固定伤害（粉伤管线 IMMUNE 阶段，仿默认票回调；
    // 记录累计在 pet 槽——下场保留；晚于本侧反弹点的固伤自然滚到下回合反弹）
    ctx->register_pink_damage_effect(
        PinkDamagePhase::IMMUNE, owner,
        [](BattleContext* wctx, int bucket_owner) {
            PinkDamageResolved& r = wctx->resolvedPink;
            if (r.target != bucket_owner || r.final <= 0) {
                return;
            }
            ElfPet& self = wctx->getPet(bucket_owner);
            if (wing_layers(self.soulmark_storage) < 4) {
                return;
            }
            const int debt = hamo_wing_get(self.soulmark_storage, kWingPinkDebtKey);
            hamo_wing_set(self.soulmark_storage, kWingPinkDebtKey, debt + r.final);
            r.final = 0;
        },
        EffectScope::ON_STAGE);
}

EffectResult wing_866_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int host_slot = ctx->find_mark_in_roster(owner, kWingMarkId);
    if (host_slot < 0) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->seerRobot[owner].elfPets[host_slot];
    wing_866_register_pipeline(ctx, owner, host);

    // 游戏开始记录全队体力（复活口径 idx=224："后场复活回到游戏开始时记录的体力"）。
    // 战前补丁（面板）已跑完 → 记录值含限定族修正，与官方一致。
    if (ctx->roundCount == 0 && hamo_wing_get(host.soulmark_storage, kWingReviveRecordedKey) == 0) {
        hamo_wing_set(host.soulmark_storage, kWingReviveRecordedKey, 1);
        for (int slot = 0; slot < 6; ++slot) {
            hamo_wing_set(host.soulmark_storage, kWingReviveHpBaseKey + slot,
                          ctx->seerRobot[owner].elfPets[slot].hp);
        }
    }

    if (hamo_wing_get(host.soulmark_storage, kWingWatchersKey) == 0) {
        hamo_wing_set(host.soulmark_storage, kWingWatchersKey, 1);

        // ── 击杀信号：≥6 层挂起 bundle + 865 技能的"击败对手则获得N层"判定位 ──
        ctx->event_center_.register_watcher(
            EventType::EVENT_DEATH, owner, ctx->roundCount, /*duration=*/0,
            /*once=*/false,
            [owner, host_slot](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx) {
                    return;
                }
                ElfPet& host = wctx->seerRobot[owner].elfPets[host_slot];
                // ① ①号效果：被击败（自己）→ 100% 随机复活背包内已被击败的 1 只
                //    （无法复活自己；被消逝的除外——is_vanished 剔除，revive_pet 双保险）
                if (ev.target == owner && ev.slot == host_slot) {
                    std::vector<int> candidates;
                    for (int slot = 0; slot < 6; ++slot) {
                        if (slot == host_slot) {
                            continue;   // 无法复活自己
                        }
                        const ElfPet& pet = wctx->seerRobot[owner].elfPets[slot];
                        if (pet.hp > 0 || wctx->is_vanished(owner, slot)) {
                            continue;
                        }
                        candidates.push_back(slot);
                    }
                    if (!candidates.empty() && g_core && g_core->revive_pet) {
                        const int slot = candidates[sbs_rand() % candidates.size()];
                        const int hp = hamo_wing_get(host.soulmark_storage,
                                                     kWingReviveHpBaseKey + slot,
                                                     resolve_pet_max_hp(
                                                         wctx->seerRobot[owner].elfPets[slot]));
                        g_core->revive_pet(wctx, owner, slot, hp);
                    }
                    return;
                }
                // ② 击杀归因（ev.actor==己方 && ev.target==对方；071/2260 契约）
                if (ev.actor != owner || ev.target != 1 - owner) {
                    return;
                }
                set_pet_int(host, kWingKillRoundKey, wctx->roundCount);  // 865 技能读
                // ③ 击杀 bundle：层数≥6 → 挂起（清零+先制+3 均下场保留）
                if (wing_layers(host.soulmark_storage) >= 6
                    && hamo_wing_get(host.soulmark_storage, kWingKillPendingKey) == 0) {
                    hamo_wing_set(host.soulmark_storage, kWingKillPendingKey, 1);
                    hamo_wing_set(host.soulmark_storage, kWingKillActiveRoundKey, -1);
                }
            },
            WatcherScope::TEAM);

        // ── 登场回合记录：bundle 的"下回合"以**非登场回合**起算（语料：重登当回合
        //    仍有一小段免控罩到下回合的回合开始控/命中前控 → 激活推迟到再下一回合）。
        //    部署不发 EVENT_ENTER_STAGE（帝君同款事实）→ 部署重生的 enter_round 保持 -1。
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration=*/0,
            /*once=*/false,
            [owner, host_slot](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.actor != owner || wctx->on_stage[owner] != host_slot) {
                    return;
                }
                hamo_wing_set(wctx->seerRobot[owner].elfPets[host_slot].soulmark_storage,
                              kWingEnterRoundKey, wctx->roundCount);
            },
            WatcherScope::TEAM);

        // ── 命中前全局锚点：击杀 bundle 的"下回合先手方命中前清零" ──
        ctx->event_center_.register_watcher(
            EventType::EVENT_BEFORE_SKILL_HIT, owner, ctx->roundCount, /*duration=*/0,
            /*once=*/false,
            [owner, host_slot](BattleContext* wctx, const BattleEvent& ev) {
                (void)ev;
                if (!wctx) {
                    return;
                }
                ElfPet& host = wctx->seerRobot[owner].elfPets[host_slot];
                if (hamo_wing_get(host.soulmark_storage, kWingKillPendingKey) != 1) {
                    return;
                }
                // 只在"已吃到先制+3 的那一回合"的命中前清零（= 击杀次回合 / 重登次回合）
                if (hamo_wing_get(host.soulmark_storage, kWingKillActiveRoundKey, -1)
                    != wctx->roundCount) {
                    return;
                }
                wing_adjust_layers(wctx, owner, host_slot, -wing_layers(host.soulmark_storage));
                hamo_wing_set(host.soulmark_storage, kWingKillPendingKey, 0);
                hamo_wing_set(host.soulmark_storage, kWingKillActiveRoundKey, -1);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 节点 W2：BATTLE_FIRST_MOVE_RIGHT（ROSTER）—— ≥5 先制+1 / 击杀 bundle 先制+3 ──
// 桶在 preemptive memset 之后跑（帝君援同款位置）；读**在场那只**的槽（先制只对
// 在场出手有意义；ROSTER 作用域保证场下挂起不丢）。
EffectResult wing_866_move_right(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->find_mark_in_roster(owner, kWingMarkId) < 0) {
        return EffectResult::kOk;
    }
    ElfPet& onstage = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    if (onstage.soulMark.id != kWingMarkId) {
        return EffectResult::kOk;
    }
    if (wing_layers(onstage.soulmark_storage) >= 5) {
        ctx->ws.preemptive_level[owner] += 1;   // ≥5：所有攻击技能先制+1
    }
    // 击杀 bundle：在场 → 先制+3 并记激活回合（本回合命中前清零）。
    // ⚠️ 登场当回合不激活（enter_round==本回合 → 推迟到下回合）：换人在 CHOOSE 点
    //    已落盘，重登当回合 MOVE_RIGHT 时它已"在场"——但语料口径是重登当回合仍持
    //    bundle（一小段免控罩到下回合），激活从**非登场回合**起算。
    if (hamo_wing_get(onstage.soulmark_storage, kWingKillPendingKey) == 1
        && hamo_wing_get(onstage.soulmark_storage, kWingEnterRoundKey, -1) != ctx->roundCount) {
        ctx->ws.preemptive_level[owner] += 3;   // 击杀后下回合所有技能先制+3
        hamo_wing_set(onstage.soulmark_storage, kWingKillActiveRoundKey, ctx->roundCount);
    }
    return EffectResult::kOk;
}

// ── 节点 W3：BATTLE_ROUND_START（STAGE）—— ≥6 加法增伤 60% 每回合重写 ──
// ws.damage_add_pct 每回合 reset → 每回合重写（776/32 套路）。只在本宠在场时写
// （side 级槽位——"自身所有攻击技能"）。
EffectResult wing_866_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& onstage = ctx->getPet(owner);
    if (onstage.soulMark.id != kWingMarkId) {
        return EffectResult::kOk;
    }
    if (wing_layers(onstage.soulmark_storage) >= 6) {
        ctx->ws.damage_add_pct[owner] += 60.0f;   // 加法增伤（语料实测口径）
    }
    return EffectResult::kOk;
}

// ── 节点 W4：FIRST/SECOND_AFTER_ACTION（STAGE）—— ≥3 回血的"自身出手起" ──
EffectResult wing_866_after_action(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->getPet(owner);
    if (host.soulMark.id == kWingMarkId) {
        set_pet_int(host, kWingActedOnceKey, 1);   // "自身首次使用过技能"（永久置位）
    }
    return EffectResult::kOk;
}

// ── 节点 W5：BATTLE_ROUND_REDUCTION_NEW_ROUND_END（STAGE）—— ≥3 回血 / ≥4 反弹 ──
EffectResult wing_866_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->getPet(owner);
    if (host.hp <= 0) {
        return EffectResult::kOk;
    }
    const int layers = wing_layers(host.soulmark_storage);
    // ≥3：每回合结束回 1/3（须出手过一次；扣减点之后——与王哈低体回血同时点）
    if (layers >= 3 && pet_int(host, kWingActedOnceKey) == 1 && g_core->heal_amount) {
        const int amount = resolve_pet_max_hp(host) / 3;
        if (amount > 0) {
            g_core->heal_amount(ctx, owner, amount);
        }
    }
    // ≥4：反弹记录的固伤（"对手行动结束后"落地为 ROUND_END 双侧桶——调研文档 §三，
    // 与官方"对手行动结束后"在后手对手场景一致、先手对手场景略晚，待实测翻案）
    const int debt = hamo_wing_get(host.soulmark_storage, kWingPinkDebtKey);
    if (debt > 0 && g_core->deal_pink_damage) {
        hamo_wing_set(host.soulmark_storage, kWingPinkDebtKey, 0);
        g_core->deal_pink_damage(ctx, 1 - owner, debt, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

// ── 战前魂印补丁（STAT_MODIFY）：限定效果"最大体力提升20%" ────────────────
// 面板事实，游戏开始前落笔（卫岳/帝君同款注册面）。满体力在场的场合保持满体力
// （当前体力同步抬升差值——限定的语义是"上限+20%"，开战满血状态不被打折）。
void hamo_715_pre_battle(BattleContext* ctx, int side, int slot) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot > 5) {
        return;
    }
    ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
    const int old_max = pet.numericalBase[NumericalPropertyIndex::HP];
    if (old_max <= 0) {
        return;
    }
    const int new_max = old_max * 120 / 100;
    pet.numericalBase[NumericalPropertyIndex::HP] = new_max;
    if (pet.hp == old_max) {
        pet.hp = new_max;
    }
}

}  // namespace

// ════════════════════════════════════════════════════════════════════
// 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +2（只增不改）
// ════════════════════════════════════════════════════════════════════
SOUL_PRE_BATTLE(kHamoMarkId, PreBattlePhase::STAT_MODIFY, hamo_715_pre_battle);
SOUL_PROGRAM(kHamoMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &hamo_715_arm,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER),
              SoulMarkNodeRef(State::BATTLE_ROUND_START, &hamo_715_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_REDUCTION_NEW_ROUND_END, &hamo_715_low_heal,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});

SOUL_PROGRAM(kWingMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &wing_866_arm,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER),
              SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &wing_866_move_right,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER),
              SoulMarkNodeRef(State::BATTLE_ROUND_START, &wing_866_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_AFTER_ACTION, &wing_866_after_action,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_AFTER_ACTION, &wing_866_after_action,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_REDUCTION_NEW_ROUND_END, &wing_866_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});
