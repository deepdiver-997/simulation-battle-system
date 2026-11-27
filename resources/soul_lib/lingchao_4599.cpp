// 灵巢之主·索杰德尔 4599 —— 命起/魂落 标记机 + 击败转移族（完整版）
//
// 官方魂印（4399 图鉴问答页 pet_id 4599；机制解析 = 语料 idx=389）：
//   ① 战斗开始时自身获得2层命起、2层魂落，战斗中二者层数之和恒为4，当其中一项层数
//      增加/减少时，另一项会同步减少/增加；自身出战时2回合内免疫己方上只精灵下场时
//      所处的异常状态，所处异常数达到4时改为所有异常状态
//   ② 对手使用威力不高于0的攻击技能时减少1层命起并令对手睡眠，未触发则免疫对手此次
//      攻击，自身每有1层命起威力阈值提升40，自身使用威力不高于0的攻击技能后减少1层
//      魂落并令对手睡眠，未触发则附加对手最大体力1/3的百分比伤害，自身每有1层魂落
//      威力阈值提升40
//   ③ 自身未击败对手时，若自身命起层数不低于魂落则恢复最大体力的1/4，魂落层数不低于
//      命起则恢复所有技能1点PP值，层数相等时己方免疫下次受到的异常状态
//   ④ 击败对手时，对手下场后所处的异常状态、能力下降状态转移给下只出战精灵
//
// 实现口径（2026-09-20，机制解析 idx=389 逐条对过）：
//   - 层数存宿主 soulmark_storage（下场保留）；**阵亡即丢**（idx=389 #1：被穿死后复活
//     层数清零，0 威力技能可再拿——EVENT_DEATH watcher 清零）。
//   - 威力阈值 = 40 ×（命起+魂落）——两条标记各写"+40 威力阈值"，和恒 4 → 恒 160。
//     「免疫威力低于 n 的攻击」的 n 取它（idx=389 #3：实际是当回合免疫威力低于阈值的
//     攻击，且**补偿盔可被穿盔响应**——引擎 BLOCK 条目按面板威力判，穿盔穿透近似 v1）。
//   - ②触发面板威力判据同 2212/854 口径（roundChoice 取技能、变威力看视图不看条件）；
//     「未触发」判定沿用米斯蒂克拍板口径：落对手身上或弹回自己身上落了都算触发。
//   - ③④互斥于击败归因（find_defeated_foe_slot）；③额外要求对手未死亡（#4）。
//   - ④转移：**裸施加**（apply_anomaly_raw，免控/免弱/抗性全穿——「转移不属于赋予」）、
//     剩余回合照搬、**剩余 <1 的 1 回合族不吃到**（#5：检测靠后）。
//   - 出战免疫的"上只精灵下场时异常"快照：死亡路径走 EVENT_DEATH（清异常前，精确）；
//     主动切换路径走回合末采样（换宠发生在操作阶段，异常自回合末不变）。
// 待做：第五技能（交换层数/未击败变威力/先制）属 moves_lib 技能线。
#include "../lingchao_layers.h"

#include "common.h"

namespace {

// ⚠️ 注册键 = effect_icon.effect_id（装配层 SoulMark.id 按 effect_id 查程序表）。
//    4599 是 monster id，只用于命名与注释——错键会让程序在真实对局挂不上
//    （2026-09-27 修复；effect_id 取自 unity 库 effect_icon 1805 行 [4599]→2193）。
constexpr int kLingchaoMarkId = 2193;
// 命起/魂落层数的存储键与"和恒 4"常量**单一真相源** = effects/lingchao_layers.h
// （技能侧 moves_lib 也 include 同一份；照 curse_stack.h 的共享层先例）。
constexpr int kLayerSum = kLingchaoLayerSum;
constexpr int kThresholdPerLayer = 40;

// 宿主 soulmark_storage / ctx plugin_storage 槽（与魂印 mark*100+n 系键错开）。
constexpr int kMingqiKey = kLingchaoMingqiKey;   // 宿主 storage：命起层数（共享头定义）
constexpr int kHunluoKey = kLingchaoHunluoKey;   // 宿主 storage：魂落层数
constexpr int kArmGuardKey = 990459923;      // plugin_storage[+side]：arm/watcher/block 接线守卫
constexpr int kLastAnomaliesKey = 990459925; // plugin_storage[+side]：上只己方宠异常快照
constexpr int kArmorRoundKey = 990459927;    // plugin_storage[+side]：补偿盔生效回合
constexpr int kArmorThresholdKey = 990459928;
// 弹控睡眠打醒旗：命起分支 REFLECTED 且弹上了自己 → 盔拦下这一击时把灵巢的睡眠清掉
// （plugin_storage[+灵巢方] = 生效回合）。
constexpr int kArmorWakeKey = 990459929;// plugin_storage[+side]：补偿盔威力阈值
constexpr int kTransferPendingBase = 990459931; // +foe：击败转移挂账
constexpr int kTransferWatchBase = 990459941;   // +foe：转移 watcher 旗

struct DefeatTransfer {
    std::vector<std::pair<int, int>> anomalies;  // {status_id, 剩余回合(≥1)}
    std::vector<std::pair<int, int>> drops;      // {stat, 负等级值(<0)}
};

// ── 层数机（和恒 4；改一边另一边镜像）────────────────────────────
int get_layers(BattleContext* ctx, int side, int slot, int which_key) {
    auto& storage = ctx->seerRobot[side].elfPets[slot].soulmark_storage;
    auto it = storage.find(which_key);
    return it == storage.end() ? 0 : any_int_or(it->second);
}

void set_layers(BattleContext* ctx, int side, int slot, int mingqi, int hunluo) {
    auto& storage = ctx->seerRobot[side].elfPets[slot].soulmark_storage;
    storage[kMingqiKey] = mingqi;
    storage[kHunluoKey] = hunluo;
}

int power_threshold(BattleContext* ctx, int side, int slot) {
    return kThresholdPerLayer
         * (get_layers(ctx, side, slot, kMingqiKey) + get_layers(ctx, side, slot, kHunluoKey));
}

// 宿主定位（任一侧；只剔消逝、阵亡照算）+ 在场门（他在场才有 ②③ 的行为；arm 是 ROSTER 不受此限）。
int host_slot_of(BattleContext* ctx, int side) {
    for (int slot = 0; slot < 6; ++slot) {
        if (ctx->seerRobot[side].elfPets[slot].soulMark.id == kLingchaoMarkId) {
            return slot;
        }
    }
    return -1;
}

// 灵巢所在方（-1 = 双方背包都没有他）。
int lingchao_side_of(BattleContext* ctx) {
    for (int side = 0; side <= 1; ++side) {
        if (host_slot_of(ctx, side) >= 0) {
            return side;
        }
    }
    return -1;
}

// 「未触发」判定（2026-09-20 用户口径修正）：**只看对手最终状态**——对手身上最终有睡眠
// 才算触发。REFLECTED（弹控把睡眠弹给灵巢自己）一律算未触发 → 上盔；且弹上去的睡眠会
// 随"对手攻击打到盔上"被打醒（BLOCK 条目的 wake 逻辑，见 kArmorWakeKey）。
// 免控/抗性挡下同理 = 未触发。
bool sleep_landed(BattleContext* ctx, int foe_side, int actor, ApplyAnomalyResult r) {
    (void)ctx;
    (void)actor;
    return r == ApplyAnomalyResult::SUCCESS || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::CONVERTED;
}

// 施加睡眠（正常通道）并返回是否落上。
bool try_sleep(BattleContext* ctx, int foe_side, int actor) {
    if (!g_core || !g_core->apply_anomaly) {
        return false;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly(
        ctx, foe_side, static_cast<int>(AbnormalStatusId::Sleep),
        /*duration_rounds=*/2, actor);
    return sleep_landed(ctx, foe_side, actor, r);
}

EffectResult soulmark_4599_power_gate(BattleContext* ctx, const EffectArgs& args);  // 前置声明：arm 镜像注册用

// ── ① 战斗开始 arm（ROSTER，幂等）：层数 2/2 + 死亡 watcher + 补偿盔 BLOCK 条目 ──
EffectResult soulmark_4599_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int slot = ctx->find_pet_with_soulmark(owner, kLingchaoMarkId);
    if (slot < 0) {
        return EffectResult::kOk;
    }
    auto& host_storage = ctx->seerRobot[owner].elfPets[slot].soulmark_storage;
    if (!host_storage.count(kMingqiKey)) {
        set_layers(ctx, owner, slot, /*mingqi=*/2, /*hunluo=*/2);
    }

    const int guard = kArmGuardKey + owner;
    if (ctx->plugin_storage.count(guard)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[guard] = true;

    // ② 镜像注册：命中前时点只跑 **mover 自己侧**的桶（FIRST/SECOND_BEFORE_SKILL_HIT
    //    的 handler 只 execute mover 桶）——命起分支在对手出手流程内，必须把 gate 节点
    //    也挂进对手侧桶才能在其命中前判定。TEAM + args 守卫（on_stage 门）保证
    //    生命周期仍锚在灵巢本人在场。
    {
        const int foe = 1 - owner;
        auto make_gate = [&](State st) {
            Effect wrapper(kLingchaoMarkId * 100 + 90, 0, foe, /*left_round=*/-1,
                           EffectArgs(std::vector<int>{owner, foe, slot}),
                           &soulmark_4599_power_gate);
            auto ce = std::make_unique<ContinuousEffect>(
                wrapper, st, foe, /*duration=*/-1, ctx->roundCount);
            ce->source_id_ = kLingchaoMarkId;
            ce->scope_ = EffectScope::TEAM;   // 锚在对手 epoch 会错清；守卫在载荷里
            ce->once_ = false;
            ctx->register_soulmark_effect(st, foe, std::move(ce));
        };
        make_gate(State::BATTLE_FIRST_BEFORE_SKILL_HIT);
        make_gate(State::BATTLE_SECOND_BEFORE_SKILL_HIT);
    }

    // 阵亡丢标记（idx=389 #1）+ 死亡路径异常快照（清异常前，比回合末采样精确）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target < 0 || ev.target > 1) {
                return;
            }
            const int side = ev.target;
            const int slot = wctx->find_mark_in_roster(side, kLingchaoMarkId);
            if (slot < 0) {
                return;
            }
            // 阵亡即丢（复活的 0 威力技能再拿，第五技能线待做）。
            set_layers(wctx, side, slot, 0, 0);
            // 死亡快照：己方这只宠下场时的异常（转给出战免疫用）。
            if (wctx->on_stage[side] == slot) {
                std::vector<int> ids;
                for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
                    if (g_core && g_core->has_active_anomaly
                        && g_core->has_active_anomaly(wctx, side, id)) {
                        ids.push_back(id);
                    }
                }
                wctx->plugin_storage[kLastAnomaliesKey + side] = ids;
            }
        },
        WatcherScope::TEAM);

    // ② 命起补偿盔：当回合免疫威力 < 阈值 的攻击（BLOCK 阶段条目，面板威力判）。
    ctx->register_damage_effect(
        DamagePhase::BLOCK, owner, DamageEffectCategory::BLOCK,
        [](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner < 0 || bucket_owner > 1) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || d.attackerId < 0 || d.attackerId > 1) {
                return;
            }
            auto rd = c->plugin_storage.find(kArmorRoundKey + bucket_owner);
            if (rd == c->plugin_storage.end()
                || any_int_or(rd->second) != c->roundCount) {
                return;   // 盔不在本回合
            }
            // 面板威力 < 阈值才挡（变威力改视图，条件看面板——854/2212 口径）。
            auto th = c->plugin_storage.find(kArmorThresholdKey + bucket_owner);
            if (th == c->plugin_storage.end()) {
                return;
            }
            const int idx = c->roundChoice[d.attackerId][1];
            if (idx < 0 || idx >= static_cast<int>(c->getPet(d.attackerId).skills.size())) {
                return;
            }
            if (c->getPet(d.attackerId).skills[static_cast<std::size_t>(idx)].power
                >= any_int_or(th->second)) {
                return;
            }
            // 弹控反射的睡眠：攻击打到盔上 → 灵巢被打醒（end_round=当前回合 → 立即失效）。
            // ⚠️ 放在伤害门前：0 威力攻击 final=0 也算"打到盔上"，官方口径照醒。
            auto wk = c->plugin_storage.find(kArmorWakeKey + bucket_owner);
            if (wk != c->plugin_storage.end()
                && any_int_or(wk->second) == c->roundCount && g_core
                && g_core->set_abnormal_status_end_round) {
                g_core->set_abnormal_status_end_round(
                    c, bucket_owner, static_cast<int>(AbnormalStatusId::Sleep), c->roundCount);
                c->plugin_storage.erase(wk);
            }
            if (!d.isRed || d.final <= 0) {
                return;
            }
            d.base = 0;
            d.afterAdd = 0;
            d.afterMul = 0;
            d.final = 0;
        });

    return EffectResult::kOk;
}

// ── ② 命中前判定（FIRST/SECOND_BEFORE_SKILL_HIT，STAGE）──────────
// 官方口径（idx=389 #2）：本效果在对方技能**命中时之前**触发——"先结算睡眠，再结算
// 霍光第五效果"。所以判定挂命中前时点：对手选完技能、威力已可知，睡眠经施加原语在
// 命中前落地；失败（免控/抗性/弹回未落）→ 立即注册当回合补偿盔，BLOCK 条目拦下这一击。
// 睡眠若落在对手出手流程内，行动开始时的控制判定已过 → 本回合照常出手、睡眠影响后续
// 回合（引擎控制检查在 ACTION_START，2026-09-20 用户问询确认）。
EffectResult soulmark_4599_power_gate(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int self_slot = resolve_subject_slot_from_args(args);
    if (owner < 0 || owner > 1 || self_slot < 0 || ctx->on_stage[owner] != self_slot) {
        return EffectResult::kOk;
    }
    // 当前出手方 = 本条命中前时点所属出手流程的 mover。
    int mover;
    if (ctx->currentState == State::BATTLE_FIRST_BEFORE_SKILL_HIT) {
        mover = ctx->is_second_mover(0) ? 1 : 0;
    } else {
        if (ctx->is_second_mover(0) == ctx->is_second_mover(1)) {
            return EffectResult::kOk;   // 先手权未结算：无第二动流程
        }
        mover = ctx->is_second_mover(0) ? 0 : 1;
    }
    const bool self_attacking = (mover == owner);
    // 只认攻击技能 + 面板威力 ≤ 0（2212/854 口径：变威力改视图，条件看面板）。
    // ⚠️ 这里**不能读 ws.skill_type_view**——它是命中时才物化的（`ON_SKILL_HIT` 写完，
    //    命中前时点仍是上一回合/默认值），会把属性技（如命诞如虚）误判成攻击技，
    //    进而错误触发魂落分支（2026-09-20 场景 101 B 组实测踩到）。读**技能静态类型**。
    const int idx = ctx->roundChoice[mover][1];
    if (idx < 0 || idx >= static_cast<int>(ctx->getPet(mover).skills.size())) {
        return EffectResult::kOk;
    }
    const Skills& used = ctx->getPet(mover).skills[static_cast<std::size_t>(idx)];
    if (used.type == SkillType::Attribute || used.power > 0) {
        return EffectResult::kOk;
    }

    const int mingqi = get_layers(ctx, owner, self_slot, kMingqiKey);
    const int hunluo = get_layers(ctx, owner, self_slot, kHunluoKey);
    const int foe = 1 - owner;

    if (!self_attacking) {
        // 命起分支：命起-1 → 睡眠 → 未触发则当回合盔（阈值 40×层和）。
        if (mingqi <= 0) {
            return EffectResult::kOk;
        }
        set_layers(ctx, owner, self_slot, mingqi - 1, kLayerSum - (mingqi - 1));
        if (!try_sleep(ctx, mover, owner)) {
            ctx->plugin_storage[kArmorRoundKey + owner] = ctx->roundCount;
            ctx->plugin_storage[kArmorThresholdKey + owner] =
                power_threshold(ctx, owner, self_slot);
            // 弹控把睡眠弹给了灵巢自己 → 盔拦下这一击时打醒他（对手最终没睡 = 未触发）。
            if (g_core && g_core->has_active_anomaly
                && g_core->has_active_anomaly(ctx, owner,
                                              static_cast<int>(AbnormalStatusId::Sleep))) {
                ctx->plugin_storage[kArmorWakeKey + owner] = ctx->roundCount;
            }
        }
    } else {
        // 魂落分支：魂落-1 → 睡眠 → 未触发则粉 1/3 最大体力。
        if (hunluo <= 0) {
            return EffectResult::kOk;
        }
        set_layers(ctx, owner, self_slot, kLayerSum - (hunluo - 1), hunluo - 1);
        if (!try_sleep(ctx, foe, owner) && g_core && g_core->deal_pink_damage) {
            const int max_hp = resolve_pet_max_hp(ctx->getPet(foe));
            g_core->deal_pink_damage(ctx, foe, max_hp / 3, DamageKind::PERCENT_VALUE, owner);
        }
    }
    return EffectResult::kOk;
}

// ── ① 出战免疫（early+STAGE 登场节点）：2 回合免疫"己方上只精灵下场时所处的异常" ──
EffectResult soulmark_4599_enter_immunity(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int self_slot = resolve_subject_slot_from_args(args);
    if (owner < 0 || owner > 1 || self_slot < 0 || ctx->on_stage[owner] != self_slot) {
        return EffectResult::kOk;
    }
    auto it = ctx->plugin_storage.find(kLastAnomaliesKey + owner);
    if (it == ctx->plugin_storage.end()) {
        return EffectResult::kOk;   // 上只精灵没带异常下场（或本局首只）
    }
    const auto ids = std::any_cast<std::vector<int>>(it->second);
    if (ids.empty()) {
        return EffectResult::kOk;
    }
    uint64_t mask = 0;
    for (const int id : ids) {
        if (id >= 0 && id <= kOfficialAbnormalStatusMaxId) {
            mask |= (1ULL << id);
        }
    }
    if (static_cast<int>(ids.size()) >= 4) {
        mask = ~0ULL;   // 异常数达到 4 → 改为所有异常状态
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), mask,
                        /*duration_rounds=*/2, /*source_id=*/kLingchaoMarkId,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/kLingchaoMarkId);
    return EffectResult::kOk;
}

// ── ① 回合末采样（ROSTER）：己方在场宠的异常快照（主动切换路径的"下场时"近似）──
EffectResult soulmark_4599_sample_anomalies(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    std::vector<int> ids;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core && g_core->has_active_anomaly
            && g_core->has_active_anomaly(ctx, owner, id)) {
            ids.push_back(id);
        }
    }
    ctx->plugin_storage[kLastAnomaliesKey + owner] = ids;
    return EffectResult::kOk;
}

// ── ③ 未击败三分支（对手未死亡才结算，idx=389 #4）──
EffectResult soulmark_4599_not_defeat(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int self_slot = resolve_subject_slot_from_args(args);
    if (owner < 0 || owner > 1 || self_slot < 0 || ctx->on_stage[owner] != self_slot) {
        return EffectResult::kOk;
    }
    if (find_defeated_foe_slot(ctx, owner) >= 0) {
        return EffectResult::kOk;   // 击败了 → 走 ④ 转移
    }
    const int foe = 1 - owner;
    if (ctx->seerRobot[foe].elfPets[ctx->on_stage[foe]].hp <= 0) {
        return EffectResult::kOk;   // 对手已死亡 → 未击败分支不结算
    }
    const int mingqi = get_layers(ctx, owner, self_slot, kMingqiKey);
    const int hunluo = get_layers(ctx, owner, self_slot, kHunluoKey);
    if (mingqi >= hunluo && g_core && g_core->heal_amount) {
        const int max_hp = resolve_pet_max_hp(ctx->seerRobot[owner].elfPets[self_slot]);
        g_core->heal_amount(ctx, owner, max_hp / 4);
    }
    if (hunluo >= mingqi) {
        ElfPet& self = ctx->seerRobot[owner].elfPets[self_slot];
        for (Skills& s : self.skills) {
            if (s.pp >= 0) {
                s.pp += 1;   // 恢复所有技能 1 点 PP（-1 = 无限制，跳过）
            }
        }
    }
    if (mingqi == hunluo) {
        // 己方免疫下次受到的异常状态（次数型次免，命中一次消费）。
        // ⚠️ source_effect_id 与出战免疫票（4599）区分——覆盖键含 subtype(同为 ANOMALY)，
        //    撞键会互相覆盖（次免 counts=1 vs 出战免疫窗口票是两笔并存的账）。
        ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                            /*duration_rounds=*/0, /*source_id=*/45990001,
                            /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/1,
                            /*source_effect_id=*/45990001);
    }
    return EffectResult::kOk;
}

// ── ④ 击败转移（对手下一只出战精灵）────────────────────────────
EffectResult soulmark_4599_defeat_transfer(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int subject_slot = resolve_subject_slot_from_args(args);
    if (owner < 0 || owner > 1 || subject_slot < 0) {
        return EffectResult::kOk;
    }
    if (ctx->is_vanished(owner, subject_slot)) {
        return EffectResult::kOk;
    }
    const int foe_slot = find_defeated_foe_slot(ctx, owner);
    if (foe_slot < 0) {
        return EffectResult::kOk;
    }
    const int foe = 1 - owner;

    DefeatTransfer pending;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (!g_core || !g_core->has_active_anomaly(ctx, foe, id)) {
            continue;
        }
        const int remain = g_core->get_abnormal_status_end_round(ctx, foe, id) - ctx->roundCount;
        if (remain < 1) {
            continue;   // 检测靠后：1 回合族（腐蚀麻痹等）吃不到击杀转移（idx=389 #5）
        }
        pending.anomalies.emplace_back(id, remain);
    }
    for (int stat = 0; stat < BattleContext::kAbilityLevelSlotCount; ++stat) {
        const int lv = ctx->ability_level(foe, stat);
        if (lv < 0) {
            pending.drops.emplace_back(stat, lv);
        }
    }

    // 挂账（重复击败覆盖：官方语义是"下只出战精灵"接最新一笔）。
    ctx->plugin_storage[kTransferPendingBase + foe] = pending;

    // 一次性登场 watcher：只挂一次（旗防重；转移完成后旗随手清）。
    const int flag_key = kTransferWatchBase + foe;
    if (!ctx->plugin_storage.count(flag_key)) {
        ctx->plugin_storage[flag_key] = true;
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, foe, ctx->roundCount,
            /*duration_rounds=*/0, /*once=*/true,
            [](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx) {
                    return;
                }
                // EVENT_ENTER_STAGE：ev.actor = 登场方（perform_switch 统一 emit，
                // drain 时新宠已完全就位）。只接对手那侧的登场。
                const int foe_side = ev.actor;
                if (foe_side < 0 || foe_side > 1) {
                    return;
                }
                const int pend_key = kTransferPendingBase + foe_side;
                auto it = wctx->plugin_storage.find(pend_key);
                if (it == wctx->plugin_storage.end()) {
                    return;
                }
                DefeatTransfer pending = std::any_cast<DefeatTransfer&>(it->second);
                wctx->plugin_storage.erase(it);
                wctx->plugin_storage.erase(kTransferWatchBase + foe_side);

                if (!g_core) {
                    return;
                }
                for (const auto& [status_id, rounds] : pending.anomalies) {
                    // 裸施加：免控/抗性/弹控/转化全穿（转移不属于赋予，idx=389 #5）。
                    g_core->apply_anomaly_raw(wctx, foe_side, status_id, rounds,
                                              /*actor=*/1 - foe_side);
                }
                for (const auto& [stat, lv] : pending.drops) {
                    g_core->stat_change(wctx, foe_side, stat, lv);   // lv<0：直接落到负值（不查免弱）
                }
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(kLingchaoMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_4599_arm,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER),
              SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_4599_enter_immunity,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_4599_sample_anomalies,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER),
              SoulMarkNodeRef(State::BATTLE_FIRST_BEFORE_SKILL_HIT, &soulmark_4599_power_gate,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_BEFORE_SKILL_HIT, &soulmark_4599_power_gate,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_OPPONENT_DEFEAT_STATUS, &soulmark_4599_not_defeat,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_OPPONENT_DEFEAT_STATUS, &soulmark_4599_defeat_transfer,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});
