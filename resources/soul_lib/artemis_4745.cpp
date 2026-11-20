// 狩猎女神·阿尔忒弥斯 4745 —— 击败类效果获取 + 狩杀之的（完整版）
//
// 官方魂印（4399 图鉴问答页 pet_id 4745；机制解析 = 语料 idx=246）：
//   ① 自身拥有己方其他精灵专属特性中的击败类效果，自身登场时每拥有一种其他精灵的
//      击败类效果获得1层狩杀之魂，若对方不存在狩杀之的则将存活的对手标记为狩杀之的
//   ② 自身拥有狩杀之魂时，每回合开始若对手的体力不低于狩杀之的的体力则当回合自身使用
//      攻击技能时消耗1层狩杀之魂并为对手与狩杀之的附加等同于威力值的真实伤害，对场下
//      精灵造成致死伤害时其保留1点体力，若对手的体力不高于狩杀之的的体力则当回合自身
//      使用攻击技能时消耗1层狩杀之魂并令自身当回合造成的固定伤害、百分比伤害提升40%，
//      同时满足时，上述效果的触发无需消耗狩杀之魂
//   ③ 狩杀之的：在场的精灵被标记为狩杀之的时体力上限减少60%直到当回合结束，然后获得
//      等同于体力值的护盾、护罩，此护盾、护罩同时存在时狩杀之的无法对阿尔忒弥斯附加
//      异常状态;新的精灵被标记为狩杀之的时，解除原本的标记（boss无效）
//
// 获取框架口径（2026-09-20 定稿，见交接文档"击杀效果线"）：无排除清单、主体槽绑定、
// 消逝断供（EVENT_VANISH 推送清副本）、狩杀之魂下场不保留。
//
// 完整版补充口径：
//   - 狩杀之的标记 = 她入场时把**存活的对手在场宠**标记（storage 记 side/slot）；
//     标记物死亡视为"不存在"（下次登场重标）；新标记解除旧标记。
//   - 上限 -60% 走 reduce_max_hp_pct 原语（削减量记账），**旧版回合结束后 1** 归还
//     （ROUND_END 会被死亡跳过，该时点恒经）；护盾/护罩 = 标记时当前体力值（source 锚定）。
//   - "两者同时存在时无法附加异常"：RuleCenter 条件回调（读标记宠两管余额）——
//     ⚠️ 回调拿不到施加者，第三方场下源施加也会被挡，v1 近似（源空间门待语料）。
//   - ②的两分支在回合开始判定并写回合旗；真伤分支在攻击 watcher 里结算：
//     威力值 = 变威力视图优先、面板兜底；标记宠在场 → 对手+标记各结算一次（双份）；
//     场下 → deal_off_field_true_damage（residue_floor=1 = 致死保留 1 点）。
//     粉伤 +40% = 粉管 AMP 条目（来源方桶）读回合旗。同时满足 → 两分支都开且不消耗。
#include "common.h"

namespace {

// ⚠️ 注册键 = effect_icon.effect_id（装配层 SoulMark.id 按 effect_id 查程序表）。
//    4745 是 monster id，只用于命名与注释——错键会让程序在真实对局挂不上
//    （2026-09-27 修复；effect_id 取自 unity 库 effect_icon 1948 行 [4745]→2354）。
constexpr int kArtemisMarkId = 2354;

// 副本 source_id 命名空间基底（真魂印 id 都是 5 位数内，永不撞）。
constexpr int kAcquiredSourceBase = 930000000;
// 狩杀之魂 storage 槽（与魂印 mark_id*100+n 系键错开，同 curse 命名风格）。
constexpr int kHuntSoulKey = 990474501;
// 消逝推送清理的接线守卫（plugin_storage，一场一次）。
constexpr int kVanishWatchKey = 990474502;
// 狩杀之的标记（她的 storage：side / slot）。
constexpr int kMarkSideKey = 990474511;
constexpr int kMarkSlotKey = 990474512;
// 上限削减归还账（她的 storage：vector<{side,slot,amount}>）。
constexpr int kRestoreListKey = 990474513;
// ②回合旗（她的 storage：0/1 + 回合戳）。
constexpr int kRoundTrueKey = 990474514;
constexpr int kRoundPinkKey = 990474515;
constexpr int kRoundNoConsumeKey = 990474516;
constexpr int kRoundStampKey = 990474517;
// 标记宠的护盾/护罩 source 锚（ShieldBank/HoodBank source_id）。
constexpr int kMarkShieldSource = 474501;
constexpr int kMarkHoodSource = 474502;

struct MaxHpRestore {
    int side = -1;
    int slot = -1;
    int amount = 0;
};

// 清掉本方两个击败时点上全部副本条目（duration=-1 非回合类，不碰
// active_round_count_，直删安全）。登场重建与消逝推送共用。
void purge_acquired_copies(BattleContext* ctx, int owner) {
    const State kill_states[2] = {State::BATTLE_OPPONENT_DEFEAT_STATUS,
                                  State::BATTLE_AFTER_DEFEATING_OPPONENT};
    for (const State st : kill_states) {
        auto& bucket = ctx->soul_mark_effects.at(st, owner);
        for (auto it = bucket.begin(); it != bucket.end();) {
            if (it->second->source_id_ >= kAcquiredSourceBase) {
                it = bucket.erase(it);
            } else {
                ++it;
            }
        }
    }
}

int hunt_soul_of(BattleContext* ctx, int side, int slot) {
    auto& storage = ctx->seerRobot[side].elfPets[slot].soulmark_storage;
    auto it = storage.find(kHuntSoulKey);
    return it == storage.end() ? 0 : any_int_or(it->second);
}

void set_hunt_soul(BattleContext* ctx, int side, int slot, int n) {
    ctx->seerRobot[side].elfPets[slot].soulmark_storage[kHuntSoulKey] = n;
}

// 当前狩杀之的（存活且未消逝才算"存在"）；无 → slot=-1。
int marked_slot_of(BattleContext* ctx, int owner, int* mark_side_out = nullptr) {
    auto& storage = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]].soulmark_storage;
    auto sit = storage.find(kMarkSideKey);
    auto lit = storage.find(kMarkSlotKey);
    if (sit == storage.end() || lit == storage.end()) {
        return -1;
    }
    const int mark_side = any_int_or(sit->second);
    const int mark_slot = any_int_or(lit->second);
    if (mark_side < 0 || mark_side > 1 || mark_slot < 0 || mark_slot > 5) {
        return -1;
    }
    ElfPet& pet = ctx->seerRobot[mark_side].elfPets[mark_slot];
    if (pet.hp <= 0 || ctx->is_vanished(mark_side, mark_slot)) {
        return -1;   // 标记物死亡/消逝 → 视为不存在
    }
    if (mark_side_out) {
        *mark_side_out = mark_side;
    }
    return mark_slot;
}

// ── ③ 标记动作：上限-60%（记账待归还）+ 护盾/护罩 = 当前体力值 ──
void apply_hunt_mark(BattleContext* ctx, int owner, int mark_side, int mark_slot) {
    auto& her = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]].soulmark_storage;
    her[kMarkSideKey] = mark_side;
    her[kMarkSlotKey] = mark_slot;

    if (g_core && g_core->reduce_max_hp_pct) {
        // 削减向上取整、同步压低当前体力；原语返回**新上限**，削减量 = 前-后（归还用）。
        ElfPet& target_before = ctx->seerRobot[mark_side].elfPets[mark_slot];
        const int before = target_before.numericalBase[NumericalPropertyIndex::HP];
        const int after = g_core->reduce_max_hp_pct(ctx, mark_side, mark_slot,
                                                    /*pct=*/60, /*floor=*/1);
        if (after > 0 && before - after > 0) {
            std::vector<MaxHpRestore> list;
            auto it = her.find(kRestoreListKey);
            if (it != her.end()) {
                list = std::any_cast<std::vector<MaxHpRestore>>(it->second);
            }
            list.push_back(MaxHpRestore{mark_side, mark_slot, before - after});
            her[kRestoreListKey] = list;
        }
    }
    ElfPet& marked = ctx->seerRobot[mark_side].elfPets[mark_slot];
    const int hp_value = marked.hp;
    marked.shield_bank_.add(ShieldBank::kSoulShieldPriority, hp_value, kMarkShieldSource,
                            /*duration=*/0);
    marked.hood_bank_.add(ShieldBank::kSoulShieldPriority, hp_value, kMarkHoodSource,
                          /*duration=*/0);
}

// ── ③ 异常免疫条件回调：标记宠护盾+护罩同时在 → 她免异常 ──
// ⚠️ 回调拿不到施加者：标记宠在场时对手施加≈标记宠施加，近似成立；
//    第三方场下源施加也会被挡（源空间门待语料补充口径）。
bool hunt_mark_anomaly_cond(const BattleContext* c, int status_id) {
    (void)status_id;
    if (!c) {
        return false;
    }
    for (int side = 0; side <= 1; ++side) {
        for (int slot = 0; slot < 6; ++slot) {
            if (c->seerRobot[side].elfPets[slot].soulMark.id != kArtemisMarkId) {
                continue;
            }
            auto& storage = c->seerRobot[side].elfPets[slot].soulmark_storage;
            auto sit = storage.find(kMarkSideKey);
            auto lit = storage.find(kMarkSlotKey);
            if (sit == storage.end() || lit == storage.end()) {
                return false;
            }
            const int mark_side = any_int_or(sit->second);
            const int mark_slot = any_int_or(lit->second);
            if (mark_side < 0 || mark_side > 1 || mark_slot < 0 || mark_slot > 5) {
                return false;
            }
            const ElfPet& marked = c->seerRobot[mark_side].elfPets[mark_slot];
            return marked.shield_bank_.quantity_of(kMarkShieldSource) > 0
                && marked.hood_bank_.quantity_of(kMarkHoodSource) > 0;
        }
    }
    return false;
}

// ── ①② 登场节点（early+STAGE）：获取副本 + 狩杀之魂 + 狩杀之的标记 + 免疫票 ──
EffectResult soulmark_4745_entrance_acquire(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int self_slot = resolve_subject_slot_from_args(args);
    if (owner < 0 || owner > 1 || self_slot < 0 || ctx->on_stage[owner] != self_slot) {
        return EffectResult::kOk;   // 非登场激活（战斗开始背包扫）不 acquisition
    }

    // ① 获取副本 + 狩杀之魂。
    // ⚠️ 莫塔里安式重触发会在**击败时点桶迭代中**重跑本节点（她的副本正在执行）——
    //    此时绝不能 purge/重注册同一个桶（erase 正在执行的条目 = use-after-free）。
    //    重触发路径只重计狩杀之魂：副本集自她登场起只可能被 EVENT_VANISH 推送清
    //    （drain 点安全），重建留到下一次真登场。
    const bool in_kill_bucket = ctx->currentState == State::BATTLE_OPPONENT_DEFEAT_STATUS
                             || ctx->currentState == State::BATTLE_AFTER_DEFEATING_OPPONENT;
    if (!in_kill_bucket) {
        purge_acquired_copies(ctx, owner);
    }

    // 扫描队友：!is_vanished（消逝断供；阵亡未消逝仍算）→ 复制其击败时点节点。
    // 副本 args = [她方, 敌方, 她的槽] + 供体魂印自带参数——载荷位置布局与
    // 原注册完全一致，读 args[2] 即主体。
    std::vector<int> contributed_marks;
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == self_slot) {
            continue;
        }
        ElfPet& mate = ctx->seerRobot[owner].elfPets[slot];
        if (ctx->is_vanished(owner, slot)) {
            continue;
        }
        SoulMark& mark = mate.soulMark;
        if (!mark.has_program_ || mark.id <= 0) {
            continue;
        }
        bool contributed = false;
        for (std::size_t idx = 0; idx < mark.program_.size(); ++idx) {
            const SoulMarkNodeRef& node = mark.program_[idx];
            if (!node.effect_fn || !is_defeat_timing_state(node.trigger_state)) {
                continue;
            }
            if (!in_kill_bucket) {
                std::vector<int> av{owner, 1 - owner, self_slot};
                for (std::size_t i = 2; i < mark.args.owned_int_args.size(); ++i) {
                    av.push_back(mark.args.owned_int_args[i]);
                }
                Effect wrapper(mark.id * 100 + static_cast<int>(idx), 0, owner,
                               /*left_round=*/-1, EffectArgs(std::move(av)), node.effect_fn);
                auto ce = std::make_unique<ContinuousEffect>(
                    wrapper, node.trigger_state, owner, /*duration=*/-1, ctx->roundCount);
                ce->source_id_ = kAcquiredSourceBase + mark.id;
                ce->scope_ = EffectScope::ON_STAGE;   // 她下场/阵亡 epoch 作废
                ce->once_ = node.once;
                ctx->register_soulmark_effect(node.trigger_state, owner, std::move(ce));
            }
            contributed = true;
        }
        if (contributed
            && std::find(contributed_marks.begin(), contributed_marks.end(), mark.id)
                   == contributed_marks.end()) {
            contributed_marks.push_back(mark.id);
        }
    }

    // ① 狩杀之魂：登场时每拥有一种击败类效果 +1 层（覆盖写，再登场重计）。
    set_hunt_soul(ctx, owner, self_slot, static_cast<int>(contributed_marks.size()));

    // ① 狩杀之的：对方不存在（无标记/标记物已死）→ 标记存活的对手在场宠。
    {
        for (auto& [k, v] : ctx->seerRobot[owner].elfPets[self_slot].soulmark_storage) {
            std::fprintf(stderr, "%d ", k);
        }
        std::fprintf(stderr, "| foe0hp=%d max=%d\n",
                     ctx->seerRobot[1].elfPets[0].hp,
                     ctx->seerRobot[1].elfPets[0].numericalBase[NumericalPropertyIndex::HP]);
    }
    if (marked_slot_of(ctx, owner) < 0) {
        const int foe = 1 - owner;
        const int foe_slot = ctx->on_stage[foe];
        ElfPet& target = ctx->seerRobot[foe].elfPets[foe_slot];
        if (target.hp > 0 && !ctx->is_vanished(foe, foe_slot)) {
            apply_hunt_mark(ctx, owner, foe, foe_slot);
        }
    }

    // ③ 免疫票：标记宠两管同时在 → 她免异常（条件回调实时判，入场授一次、覆盖键刷新）。
    ctx->rule_center_.grant_immune(
        owner, static_cast<int>(ImmunityType::ANOMALY), coverage_all(), /*anomaly_mask=*/0,
        /*duration_rounds=*/0, ctx->roundCount, /*source_id=*/0, /*soul=*/false,
        EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/0,
        /*source_effect_id=*/kArtemisMarkId, static_cast<int>(ImmunityTier::Modern),
        hunt_mark_anomaly_cond);

    // ② 粉伤 +40% 条目（来源方桶，一场一次；是否生效读回合旗）。
    const int pink_key = 990474518 + owner;
    if (!ctx->plugin_storage.count(pink_key)) {
        ctx->plugin_storage[pink_key] = true;
        ctx->register_default_pink_effect(
            PinkDamagePhase::AMP, owner,
            [](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner < 0 || bucket_owner > 1) {
                    return;
                }
                // 增粉从来源方桶读：只增幅她打出去的粉。
                PinkDamageResolved& r = c->resolvedPink;
                if (r.actor != bucket_owner || r.final <= 0) {
                    return;
                }
                auto& storage = c->seerRobot[bucket_owner]
                                     .elfPets[c->on_stage[bucket_owner]].soulmark_storage;
                auto stamp = storage.find(kRoundStampKey);
                auto flag = storage.find(kRoundPinkKey);
                if (stamp == storage.end() || flag == storage.end()) {
                    return;
                }
                if (any_int_or(stamp->second) != c->roundCount
                    || any_int_or(flag->second) <= 0) {
                    return;
                }
                r.final = r.final * 140 / 100;
            });
    }

    // ② 真伤触发 watcher（一场一次；回合旗+狩杀之魂在回调里守卫）。
    const int true_key = 990474528 + owner;
    if (!ctx->plugin_storage.count(true_key)) {
        ctx->plugin_storage[true_key] = true;
        ctx->event_center_.register_watcher(
            EventType::EVENT_HIT, owner, ctx->roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.actor < 0 || ev.actor > 1 || !g_core) {
                    return;
                }
                const int host = c->find_mark_in_roster(ev.actor, kArtemisMarkId);
                if (host < 0 || c->on_stage[ev.actor] != host) {
                    return;   // 她不在场不结算
                }
                if (c->ws.skill_type_view[ev.actor] == static_cast<int>(SkillType::Attribute)) {
                    return;   // 只认攻击技能
                }
                auto& storage = c->seerRobot[ev.actor].elfPets[host].soulmark_storage;
                auto stamp = storage.find(kRoundStampKey);
                if (stamp == storage.end()
                    || any_int_or(stamp->second) != c->roundCount) {
                    return;   // 本回合未判定（无狩杀之魂/无标记）
                }
                auto tflag = storage.find(kRoundTrueKey);
                if (tflag == storage.end() || any_int_or(tflag->second) <= 0) {
                    return;   // 真伤分支未开
                }
                // 消耗 1 层狩杀之魂（同时满足分支不消耗）。
                auto nc = storage.find(kRoundNoConsumeKey);
                const bool no_consume = nc != storage.end()
                                     && any_int_or(nc->second) > 0;
                const int left = hunt_soul_of(c, ev.actor, host);
                if (left <= 0) {
                    return;   // 层已耗尽
                }
                if (!no_consume) {
                    set_hunt_soul(c, ev.actor, host, left - 1);
                }
                // 威力值：变威力视图优先，面板兜底（视图 value=-1 = 未物化）。
                const int idx = c->roundChoice[ev.actor][1];
                int power = 0;
                if (idx >= 0 && idx < static_cast<int>(c->getPet(ev.actor).skills.size())) {
                    power = c->getPet(ev.actor).skills[static_cast<std::size_t>(idx)].power;
                }
                const auto& view = c->ws.skill_power_view[ev.actor];
                if (view.rewritten && view.value >= 0) {
                    power = view.value;
                }
                if (power <= 0) {
                    return;
                }
                const int foe = 1 - ev.actor;
                // 一次对在场附加；一次对被标记的印记附加（标记宠在场=同只 → 双份，idx=246）。
                if (g_core->deal_true_damage) {
                    g_core->deal_true_damage(c, foe, power, ev.actor);
                }
                int mark_side = -1;
                const int mark_slot = marked_slot_of(c, ev.actor, &mark_side);
                if (mark_side < 0 || mark_slot < 0) {
                    return;
                }
                if (mark_side == foe && mark_slot == c->on_stage[foe]) {
                    if (g_core->deal_true_damage) {
                        g_core->deal_true_damage(c, foe, power, ev.actor);
                    }
                    return;
                }
                // 场下标记宠：致死保留 1 点体力（residue_floor=1）。
                if (g_core->deal_off_field_true_damage) {
                    g_core->deal_off_field_true_damage(c, mark_side, mark_slot, power,
                                                       ev.actor, /*residue_floor=*/1);
                }
            },
            WatcherScope::TEAM);
    }

    // ① 消逝推送清理（一场一次，幂等）：她在场期间队友被消逝 → 立即清副本，
    //    不等下次登场（官方"队友被消逝就不能享受到击杀收益"是即时断供）。
    //    狩杀之魂**已获得的层数不清**（登场获得口径），只断后续——下次登场重计。
    //    消逝必发 EVENT_VANISH（071 不变量），推送注销无需对账（§4.2.6 同款论证）。
    if (!ctx->plugin_storage.count(kVanishWatchKey)) {
        ctx->plugin_storage[kVanishWatchKey] = true;
        ctx->event_center_.register_watcher(
            EventType::EVENT_VANISH, owner, ctx->roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.target < 0 || ev.target > 1) {
                    return;
                }
                const int owner = ev.target;
                for (int slot = 0; slot < 6; ++slot) {
                    if (wctx->seerRobot[owner].elfPets[slot].soulMark.id == kArtemisMarkId) {
                        purge_acquired_copies(wctx, owner);
                        return;
                    }
                }
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── ② 回合开始判定：对手体力 vs 狩杀之的体力 → 两分支回合旗（同时满足不消耗）──
EffectResult soulmark_4745_round_judge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int self_slot = resolve_subject_slot_from_args(args);
    if (owner < 0 || owner > 1 || self_slot < 0 || ctx->on_stage[owner] != self_slot) {
        return EffectResult::kOk;
    }
    auto& storage = ctx->seerRobot[owner].elfPets[self_slot].soulmark_storage;
    // 先清回合旗（上一轮的作废）。
    storage[kRoundTrueKey] = 0;
    storage[kRoundPinkKey] = 0;
    storage[kRoundNoConsumeKey] = 0;
    storage[kRoundStampKey] = ctx->roundCount;

    if (hunt_soul_of(ctx, owner, self_slot) <= 0) {
        return EffectResult::kOk;   // 没有狩杀之魂：②整条不触发
    }
    int mark_side = -1;
    const int mark_slot = marked_slot_of(ctx, owner, &mark_side);
    if (mark_slot < 0) {
        return EffectResult::kOk;   // 没有存活的狩杀之的可比
    }
    const int foe = 1 - owner;
    const int opp_hp = ctx->seerRobot[foe].elfPets[ctx->on_stage[foe]].hp;
    const int mark_hp = ctx->seerRobot[mark_side].elfPets[mark_slot].hp;
    // 不低于 → 真伤分支；不高于 → 粉增伤分支；相等（含标记宠就是在场对手）→ 双开不消耗。
    if (opp_hp >= mark_hp) {
        storage[kRoundTrueKey] = 1;
    }
    if (opp_hp <= mark_hp) {
        storage[kRoundPinkKey] = 1;
    }
    storage[kRoundNoConsumeKey] = (opp_hp == mark_hp) ? 1 : 0;
    return EffectResult::kOk;
}

// ── ③ 上限归还（旧版回合结束后 1：死亡跳过 ROUND_END 时该时点仍恒经）──
EffectResult soulmark_4745_restore_max_hp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    for (int slot = 0; slot < 6; ++slot) {
        if (ctx->seerRobot[owner].elfPets[slot].soulMark.id != kArtemisMarkId) {
            continue;
        }
        auto& storage = ctx->seerRobot[owner].elfPets[slot].soulmark_storage;
        auto it = storage.find(kRestoreListKey);
        if (it == storage.end()) {
            continue;
        }
        auto list = std::any_cast<std::vector<MaxHpRestore>>(it->second);
        storage.erase(it);
        if (g_core && g_core->raise_max_hp_flat) {
            for (const MaxHpRestore& r : list) {
                const int rc = g_core->raise_max_hp_flat(ctx, r.side, r.slot, r.amount);
            }
        }
    }
    return EffectResult::kOk;
}

// 离场钩子：狩杀之魂下场不保留（官方实测）。手动扫槽定位（find_pet_with_soulmark
// 的 hp>0 门会把阵亡的她剔掉，而阵亡下场同样要清）。
void artemis_exit_clear_hunt_soul(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    for (int slot = 0; slot < 6; ++slot) {
        ElfPet& pet = ctx->seerRobot[owner].elfPets[slot];
        if (pet.soulMark.id == kArtemisMarkId) {
            set_hunt_soul(ctx, owner, slot, 0);
            pet.soulmark_storage.erase(kRoundStampKey);
        }
    }
}

}  // namespace

// 程序链路：登场（获取+标记+免疫）、回合开始（②判定）、旧回合末1（③上限归还）。
SOUL_PROGRAM(kArtemisMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_4745_entrance_acquire,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_4745_round_judge,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_OLD_ROUND_END_1, &soulmark_4745_restore_max_hp,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER)});
SOUL_MARK_HOOKS(kArtemisMarkId, nullptr, &artemis_exit_clear_hunt_soul);
