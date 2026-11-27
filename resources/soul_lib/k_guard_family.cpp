// K1 挡伤/减伤/免致命族 —— effect 3/150/254/260/280/317/329/337/766（9 宏 / 21 宠 19 行）
//
// 行清单（tools/soul_effects_dump.py 3 150 254 260 280 317 329 337 766，2026-10-01）：
//   3   减伤%：2419 塔西姆 10(无参) / 2418 西姆 10(无参) / 2438 火舞天尊 15(无参) /
//       2867 森林弓骑兵 args[50,0]→50                          → REDUCE_PCT（红伤）
//   150 免致命：2468 万魔天尊 50%(无参) / [2378,2379] 圣婴圣皇 args[350,0]→35% /
//       4837 保姆派派 args[600,0]→60%（boss有效——PvP 引擎忽略，S1 §七口径）
//                                                            → BLOCK 票，isCrit 门 + 概率
//   254 减伤%：[2326,2327] 瑞儿瑞亚 "受到所有伤害减少5%"(无参) → 红伤 REDUCE_PCT
//       + **粉伤** REDUCE_EXTRA（"所有伤害"措辞 vs 3 的"攻击伤害"——两管线都挂）
//   260 减伤%：2424 鲍尔 10(无参)；2425 鲍尔特 "10%闪避对手的攻击" → 无减伤、闪避子句
//       2026-10-04 K5 落地（soulmark_260_dodge_round_start，S1-P0 闪避钩子）
//   280 减伤%+闪避：2508 焱武圣尊 args[15,5]→减伤15%（闪避 5% 亦已 K5 落地，db[1]）/
//       3077 神鹿仙灵 args[30,0]→30
//   317 减伤%：2638 丘比特 args[100,10]→10 / 2710 圣光·奥尔德 args[100,25]→25（pct 在 db[1]）
//   329 条件点减：[2673,2693] 斩月双生 args[1,100] "若本回合没有受到伤害，则下回合受到的
//       伤害减少100点" → ROUND_START 翻旗 + TAKE_DAMAGE/TAKE_PINK_DAMAGE 记伤 + REDUCE_FLAT
//   337 条件点减：2683 埃尔文达 args[2,5,100] "若对手处于冻伤状态，则自身本回合受到的
//       伤害减少100点" → has_active_abnormal_status(对手, db[1]=5 冻伤) 门 + REDUCE_FLAT
//   766 锁伤 CAP：3107 格罗姆特 args[260,1,100,50] "自身体力低于对手时，受到的伤害不超过
//       260（boss无效）" + 第二子句"攻击附加造成伤害 50%~100%（对手体力越高比例越高）固伤"；
//       4836 观星派派 args[400,1] "回合开始时若自身体力低于对手则当回合受到的攻击伤害
//       不超过400" → **管线 CAP 阶段首批消费者**（头文件注记的"零注册"在此落地），
//       消费 697 ignore_damage_limit 凭证（"无视伤害限制"穿锁伤）
//
// ⚠️ 三查记录（2026-10-01）：
//   ① 内核无这 9 个 id 的硬编码（grep src/include 零命中）→ 无假阳性风险；
//   ② effect_info 3/150/766 是**技能效果空间**的官方模板（解除能力下降/降防/消强化），
//     与魂印空间无关——魂印语义权威 = effect_icon.tips（K 工单口径），args 语义以行 dump 为准；
//   ③ 全库无同 id 注册（grep SOUL_MARK/SOUL_PROGRAM 零命中），无双领。
//
// "行≠宏"参数解析：运行时 args[3..] = DB args[2:]（bind_soulmark_args 丢前两位），
// 而本族行数值**大多恰好落在 DB[0..1]** → 一律回读宿主 soulMark.args 全量（real battle
// 由 pet_factory 现代链路填充、场景手工构造）；空参行按 monster_id 查表（值为行 dump 原文）。
#include "common.h"

#include <effects/pink_damage_pipeline.h>

namespace {

// ── 宿主行参数视图（"行≠宏"：全量 DB args + monster_id）──────────────────────

struct HostRow {
    const std::vector<int>* db = nullptr;   // 宿主 soulMark 全量 DB args（bind 前）
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
        slot = ctx->find_pet_with_soulmark(owner, mark_id);   // 场景兜底（mozun 同款）
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

// ── arm 幂等守卫（每 epoch 一次）────────────────────────────────────────────
// 战斗开始时 early 的 ENTER_EXIT_STAGE 节点会跑**两次**（activate 立即执行一次 +
// init_battle 的 ENTER_EXIT 桶再执行一次）→ 不设防则管线同 epoch 注册两条（减伤翻倍）。
// 以 pipeline_valid_id 为 epoch：开局两次触发同 epoch → 第二次跳过；换宠重上场
// epoch 已递增 → 自然重挂（旧条目已随切换作废）。
struct EpochGuard {
    int armed_epoch = -1;
};

bool already_armed_this_epoch(BattleContext* ctx, int owner, int storage_key) {
    ElfPet& pet = ctx->getPet(owner);
    if (!pet.soulmark_storage.count(storage_key)) {
        pet.soulmark_storage[storage_key] = EpochGuard{};
    }
    EpochGuard& guard = std::any_cast<EpochGuard&>(pet.soulmark_storage[storage_key]);
    if (guard.armed_epoch == ctx->pipeline_valid_id[owner]) {
        return true;
    }
    guard.armed_epoch = ctx->pipeline_valid_id[owner];
    return false;
}

// 空参行按 monster_id 查表（值 = 行 dump 原文）；查不到 = 该行语义本插件不做（记档）→ 不注册。
int row_pct(const HostRow& row, int db_idx,
            std::initializer_list<std::pair<int, int>> monster_table) {
    const int from_db = db_at(row, db_idx, -1);
    if (from_db >= 0) {
        return from_db;
    }
    for (const auto& [monster, pct] : monster_table) {
        if (row.monster_id == monster) {
            return pct;
        }
    }
    return -1;
}

// 在场守卫：管线/watcher 回调里防陈旧条目误伤（宿主不在场/已换魂印即失效）。
// 正常生命周期由 pipeline_valid_id epoch 兜底，这里是第二道保险（sarika 同款纪律）。

// ── 通用件：百分比减伤（红伤 REDUCE_PCT；可选粉伤 REDUCE_EXTRA）──────────────

void arm_percent_mitigate(BattleContext* ctx, const EffectArgs& args, int mark_id,
                          int db_idx,
                          std::initializer_list<std::pair<int, int>> monster_table,
                          bool include_pink) {
    if (!ctx) {
        return;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return;
    }
    if (already_armed_this_epoch(ctx, owner, mark_id * 100 + 9)) {
        return;   // 开局 early 节点双触发防重（见 EpochGuard）
    }
    const HostRow row = host_row(ctx, args, mark_id);
    const int pct = row_pct(row, db_idx, monster_table);
    if (pct <= 0) {
        return;   // 无参且 monster 不在表（如 260 鲍尔特闪避行）→ 记档跳过
    }
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [mark_id, pct](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, mark_id)) {
                return;
            }
            d.final = d.final * (100 - pct) / 100;
        });
    if (include_pink) {
        // "受到**所有**伤害减少X%"：粉伤走 REDUCE_EXTRA（百分比免减，乘法，与抗性连乘）。
        ctx->register_pink_damage_effect(
            PinkDamagePhase::REDUCE_EXTRA, owner,
            [mark_id, pct](BattleContext* c, int bucket_owner) {
                PinkDamageResolved& p = c->resolvedPink;
                if (p.target != bucket_owner || p.final <= 0) {
                    return;
                }
                if (!onstage_has_soul_mark(c, bucket_owner, mark_id)) {
                    return;
                }
                p.final = p.final * (100 - pct) / 100;
            });
    }
}

// ── effect 3 / 254 / 260 / 280 / 317：百分比减伤 ─────────────────────────────

EffectResult soulmark_3_mitigate(BattleContext* ctx, const EffectArgs& args) {
    arm_percent_mitigate(ctx, args, 3, 0, {{2418, 10}, {2419, 10}, {2438, 15}}, false);
    return EffectResult::kOk;
}

EffectResult soulmark_254_mitigate(BattleContext* ctx, const EffectArgs& args) {
    arm_percent_mitigate(ctx, args, 254, 0, {{2326, 5}, {2327, 5}}, true);
    return EffectResult::kOk;
}

EffectResult soulmark_260_mitigate(BattleContext* ctx, const EffectArgs& args) {
    // 2425 鲍尔特"10%闪避"行不在表 → 不注册（P0 命中钩子缺口，文件头记档）。
    arm_percent_mitigate(ctx, args, 260, 0, {{2424, 10}}, false);
    return EffectResult::kOk;
}

EffectResult soulmark_280_mitigate(BattleContext* ctx, const EffectArgs& args) {
    // 2508 的 db[1]=5（闪避5%）不消费——同鲍尔特记档。
    arm_percent_mitigate(ctx, args, 280, 0, {{3077, 30}, {2508, 15}}, false);
    return EffectResult::kOk;
}

EffectResult soulmark_317_mitigate(BattleContext* ctx, const EffectArgs& args) {
    arm_percent_mitigate(ctx, args, 317, 1, {{2638, 10}, {2710, 25}}, false);
    return EffectResult::kOk;
}

// ── effect 150：概率免疫致命一击伤害（暴击门 BLOCK 票）───────────────────────
// pct：args[350,0]→35 / [600,0]→60（db[0]/10）；无参行 2468 万魔天尊 → 50。
// 概率测试口径：场景用 db[0]=1000（=100%）/0 做确定性两档，真实行只是不同常数。

EffectResult soulmark_150_crit_immune(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (already_armed_this_epoch(ctx, owner, 150 * 100 + 9)) {
        return EffectResult::kOk;   // 开局 early 节点双触发防重（见 EpochGuard）
    }
    const HostRow row = host_row(ctx, args, 150);
    int permille = db_at(row, 0, -1);
    if (permille < 0) {
        permille = (row.monster_id == 2468) ? 500 : -1;
    }
    if (permille < 0) {
        return EffectResult::kOk;
    }
    const int pct = permille / 10;
    ctx->register_damage_effect(
        DamagePhase::BLOCK, owner, DamageEffectCategory::BLOCK,
        [pct](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            // "致命一击"= 暴击（引擎 isCrit；非"瞬杀/秒杀"——后者无伤害量可免）。
            if (d.defenderId != bucket_owner || !d.isRed || !d.isCrit || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, 150)) {
                return;
            }
            if (!roll_percent(c->rng(), pct)) {
                return;
            }
            d.base = 0;
            d.afterAdd = 0;
            d.afterMul = 0;
            d.final = 0;
            d.addPct = 0;
            d.mulCoef = 0.0;
        });
    return EffectResult::kOk;
}

// ── effect 329 斩月双生：本回合未受伤 → 下回合受伤减少 N 点 ─────────────────
// args[1,100]：db[0]=窗口回合（官方文本固定"下回合"，不消费）、db[1]=点数。
// 三件套：TAKE_DAMAGE/TAKE_PINK_DAMAGE watcher 记伤（TEAM 永久 + 在场守卫）；
// ROUND_START 翻旗（上回合未受伤 → 本回合减）；REDUCE_FLAT 条目消费旗。

struct ZhanYueRuntime {
    int armed_epoch = -1;          // arm 每 epoch 一次（开局 early 节点跑两次，见 EpochGuard）
    bool took_damage_this_round = false;
    bool reduce_this_round = false;
    bool saw_full_round = false;   // 第 1 回合开始时"还没受伤"≠"上回合没受伤"——未过完整回合不翻旗
    bool watchers_armed = false;   // 记伤 watcher 只挂一次（storage 随宠重建 → 每场一次）
};

ZhanYueRuntime& zhanyue_runtime(BattleContext* ctx, int owner) {
    ElfPet& pet = ctx->getPet(owner);
    constexpr int kSlotKey = 329 * 100 + 1;
    if (!pet.soulmark_storage.count(kSlotKey)) {
        pet.soulmark_storage[kSlotKey] = ZhanYueRuntime{};
    }
    return std::any_cast<ZhanYueRuntime&>(pet.soulmark_storage[kSlotKey]);
}

EffectResult soulmark_329_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 329);
    const int amount = db_at(row, 1, 100);
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    ZhanYueRuntime& rt = zhanyue_runtime(ctx, owner);
    if (rt.armed_epoch == ctx->pipeline_valid_id[owner]) {
        return EffectResult::kOk;   // 同 epoch 二次触发（开局桶+activate）不复位旗/不重注册
    }
    rt.armed_epoch = ctx->pipeline_valid_id[owner];
    rt.took_damage_this_round = false;   // 中途登场：本回合既往未记伤
    rt.reduce_this_round = false;        // 减伤旗只能由完整回合的 ROUND_START 翻出

    ctx->register_damage_effect(
        DamagePhase::REDUCE_FLAT, owner, DamageEffectCategory::MITIGATE,
        [amount](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, 329)) {
                return;
            }
            ZhanYueRuntime& rt2 = zhanyue_runtime(c, bucket_owner);
            if (rt2.reduce_this_round && d.final > 0) {
                d.final = std::max(0, d.final - amount);
            }
        });

    // 记伤 watcher 只挂一次（TEAM 永久；回调里查在场，换宠/下场自然哑火）。
    if (rt.watchers_armed) {
        return EffectResult::kOk;
    }
    rt.watchers_armed = true;
    auto record_damage = [owner](BattleContext* wctx, const BattleEvent& ev) {
        if (ev.target != owner) {
            return;
        }
        if (!onstage_has_soul_mark(wctx, owner, 329)) {
            return;
        }
        zhanyue_runtime(wctx, owner).took_damage_this_round = true;
    };
    ctx->event_center_.register_watcher(EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount,
                                        /*duration_rounds=*/0, /*once=*/false, record_damage,
                                        WatcherScope::TEAM);
    ctx->event_center_.register_watcher(EventType::EVENT_TAKE_PINK_DAMAGE, owner,
                                        ctx->roundCount, /*duration_rounds=*/0, /*once=*/false,
                                        record_damage, WatcherScope::TEAM);
    return EffectResult::kOk;
}

EffectResult soulmark_329_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, owner, 329)) {
        return EffectResult::kOk;
    }
    ZhanYueRuntime& rt = zhanyue_runtime(ctx, owner);
    rt.reduce_this_round = rt.saw_full_round && !rt.took_damage_this_round;
    rt.saw_full_round = true;
    rt.took_damage_this_round = false;
    return EffectResult::kOk;
}

// ── effect 337 埃尔文达：对手处于冻伤 → 自身受伤减少 N 点 ────────────────────
// args[2,5,100]：db[0]=2（目标码"对手"，文本固定不消费）、db[1]=异常状态 id（5=冻伤，
// AbnormalStatusId::Frostbite）、db[2]=点数。

EffectResult soulmark_337_flat(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (already_armed_this_epoch(ctx, owner, 337 * 100 + 9)) {
        return EffectResult::kOk;   // 开局 early 节点双触发防重（见 EpochGuard）
    }
    const HostRow row = host_row(ctx, args, 337);
    const int status_id = db_at(row, 1, static_cast<int>(AbnormalStatusId::Frostbite));
    const int amount = db_at(row, 2, 100);
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    ctx->register_damage_effect(
        DamagePhase::REDUCE_FLAT, owner, DamageEffectCategory::MITIGATE,
        [status_id, amount](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, 337)) {
                return;
            }
            // 判定式 = has_active_abnormal_status 同款（内核符号插件不可链，直读公共字段）：
            //   roundCount < abnormal_status_end_round[side][status] 即在效期内。
            if (c->roundCount >= c->abnormal_status_end_round[1 - bucket_owner][status_id]) {
                return;
            }
            d.final = std::max(0, d.final - amount);
        });
    return EffectResult::kOk;
}

// ── effect 766：伤害锁定 CAP（管线 CAP 阶段首批消费者）──────────────────────
// 3107 格罗姆特：受伤时"自身体力低于对手"→ 锁 260；附加子句：攻击造成伤害的
//   50%~100%（对手体力越高比例越高）粉伤追击（raw_attack_damage 422 族口径）。
// 4836 观星派派：ROUND_START 评"自身体力低于对手"→ 本回合锁 400（行≠宏：条件评估
//   时点不同，按 monster_id 分支）。
// 两行都注"boss无效"——PvP 引擎按 S1 §七口径忽略。697"无视伤害限制"凭证穿锁伤。

struct LockRuntime {
    int armed_epoch = -1;            // arm 每 epoch 一次（开局 early 节点跑两次，见 EpochGuard）
    bool round_cap_active = false;   // 4836：ROUND_START 评出的本回合锁伤开关
    bool drain_watcher_armed = false;   // 格罗姆特追击 watcher 只挂一次（每场）
};

LockRuntime& lock_runtime(BattleContext* ctx, int owner) {
    ElfPet& pet = ctx->getPet(owner);
    constexpr int kSlotKey = 766 * 100 + 1;
    if (!pet.soulmark_storage.count(kSlotKey)) {
        pet.soulmark_storage[kSlotKey] = LockRuntime{};
    }
    return std::any_cast<LockRuntime&>(pet.soulmark_storage[kSlotKey]);
}

EffectResult soulmark_766_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    LockRuntime& lrt = lock_runtime(ctx, owner);
    if (lrt.armed_epoch == ctx->pipeline_valid_id[owner]) {
        return EffectResult::kOk;   // 同 epoch 二次触发不重注册（开局桶+activate）
    }
    lrt.armed_epoch = ctx->pipeline_valid_id[owner];
    const HostRow row = host_row(ctx, args, 766);
    const int cap = db_at(row, 0, 260);
    const bool round_start_cond = (row.monster_id == 4836);   // 行≠宏分支
    const int drain_pct_min = db_at(row, 3, 50);
    const int drain_pct_max = db_at(row, 2, 100);

    ctx->register_damage_effect(
        DamagePhase::CAP, owner, DamageEffectCategory::MITIGATE,
        [cap, round_start_cond](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            // "受到的(攻击)伤害不超过N"：只锁红伤（粉伤有独立 CAP 阶段，本行无此语义）。
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= cap) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, 766)) {
                return;
            }
            if (d.attackerId >= 0 && d.attackerId <= 1
                && c->ws.attack_credential[d.attackerId].ignore_damage_limit) {
                return;   // 官方 697"无视伤害限制"穿锁伤（凭证首个消费点）
            }
            bool active = false;
            if (round_start_cond) {
                active = lock_runtime(c, bucket_owner).round_cap_active;
            } else {
                // 格罗姆特：受伤当下比较双方在场体力（文本无"回合开始时"前缀）。
                const ElfPet& self = c->getPet(bucket_owner);
                const ElfPet& foe = c->getPet(1 - bucket_owner);
                active = self.hp > 0 && self.hp < foe.hp;
            }
            if (active) {
                d.final = cap;
            }
        });

    // 格罗姆特第二子句：攻击追击粉伤（watcher 只挂一次，TEAM 永久 + 在场守卫）。
    if (round_start_cond) {
        return EffectResult::kOk;
    }
    if (lrt.drain_watcher_armed) {
        return EffectResult::kOk;
    }
    lrt.drain_watcher_armed = true;
    const int foe_side = 1 - owner;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, foe_side, drain_pct_min, drain_pct_max](BattleContext* wctx,
                                                        const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || ev.target != foe_side) {
                return;   // 只认"自己攻击对手"造成的伤害
            }
            if (!onstage_has_soul_mark(wctx, owner, 766)) {
                return;
            }
            // 只认攻击伤害时点（固定/真伤/粉伤不构成"攻击造成伤害"，mozun 同款口径）。
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                return;
            }
            const int raw = wctx->ws.raw_attack_damage[owner];
            if (raw <= 0) {
                return;
            }
            const ElfPet& foe = wctx->getPet(ev.target);
            if (foe.hp <= 0) {
                return;
            }
            const int foe_max = resolve_pet_max_hp(foe);
            // 比例随对手体力线性：满体力 = max%（100），趋零 = min%（50）。
            const int pct = foe_max > 0
                ? drain_pct_min + (drain_pct_max - drain_pct_min) * foe.hp / foe_max
                : drain_pct_min;
            const int amount = raw * pct / 100;
            if (amount > 0 && g_core && g_core->deal_pink_damage) {
                g_core->deal_pink_damage(wctx, ev.target, amount, DamageKind::FIXED, owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

EffectResult soulmark_766_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, owner, 766)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 766);
    if (row.monster_id != 4836) {
        return EffectResult::kOk;   // 格罗姆特无"回合开始时"条件，不吃这个节点
    }
    LockRuntime& rt = lock_runtime(ctx, owner);
    const ElfPet& self = ctx->getPet(owner);
    const ElfPet& foe = ctx->getPet(1 - owner);
    rt.round_cap_active = self.hp > 0 && self.hp < foe.hp;
    return EffectResult::kOk;
}

// ── 260/280 闪避子句补全（K5，2026-10-04；S1-P0 闪避钩子解锁 K1 记档）────────
// ws.dodge_rate 每回合 reset 归零 → ROUND_START 节点重写（k_hit_dodge_aura.cpp 头注
// 时点依据）。闪避是减算（hit_chance = accuracy - dodge×100），必中技 accuracy_fix_100
// 天然绕过 → 不影响本文件既有 REDUCE_PCT 断言（940 用必中燕返探针，回归安全）。
EffectResult soulmark_280_dodge_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 280)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 280);
    const int pct = db_at(row, 1, -1);   // 2508 焱武圣尊 db[1]=5；3077 db[1]=0 → 不写
    if (pct > 0) {
        ctx->ws.dodge_rate[owner] = pct / 100.0f;
    }
    return EffectResult::kOk;
}

EffectResult soulmark_260_dodge_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 260)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 260);
    // 2425 鲍尔特 args空「10%闪避对手的攻击」→ monster 表（2424 鲍尔无闪避子句）。
    const int pct = row.monster_id == 2425 ? 10 : -1;
    if (pct > 0) {
        ctx->ws.dodge_rate[owner] = pct / 100.0f;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +9（只增不改）────────────────
// 全部走"early=true 的 ENTER_EXIT_STAGE arm 节点"：每次登场重跑一次（2343 语义批次
// 口径），管线注册天然幂等——下场 epoch 先作废旧条目（perform_switch ①），再上场
// 重注册（④）。329/766 另挂 ROUND_START 节点做回合边界翻旗。
SOUL_PROGRAM(3, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_3_mitigate,
                  /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(150, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_150_crit_immune,
                    /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(254, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_254_mitigate,
                    /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(260, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_260_mitigate,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_260_dodge_round_start)});
SOUL_PROGRAM(280, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_280_mitigate,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_280_dodge_round_start)});
SOUL_PROGRAM(317, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_317_mitigate,
                    /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(329, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_329_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_329_round_start)});
SOUL_PROGRAM(337, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_337_flat,
                    /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(766, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_766_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_766_round_start)});
