// 灵巢之主·索杰德尔 4599 技能组（2026-09-20）
//
// 官方技能全文 = reference/scrape_4399/out/pets_full.jsonl（灵巢之主·索杰德尔节）；
// 效果模板 = DB effect_info。命起/魂落层数走共享层 effects/lingchao_layers.h（与魂印侧同源）。
//
// 已实现：
//   37083 迴还（先三 90 必中）：1164 反转对手提升，未满足/失败则消除提升+回合类 /
//        1056(已有) 反转自身下降→成功则2回合对手属性技无效 / 1686 先手增伤(对手体力高额外)
//   37084 碎魂之爪（120）：1581 对手体力低于1/2 → 伤害 +200%
//   37085 凝魂之息（130）：452 3回合内所有攻击伤害为自己回血
//   37089 蒙尘古音（80）：933 未击败则己方下2次攻击必暴
//   28582 千魂之结：712 3回合内将对手攻/特攻能力置零
//   28583 命诞如虚：2106 命起+1（镜像魂落）+ 命起高于魂落时回血并附加等量百分比
//   28584 魂殒若梦：2107 魂落+1（镜像命起）+ 魂落高于命起时消逝对手场下阵亡精灵
//   37087 无念归空净（150）：1952 无视对手提升→异常 / 2079 对手有护盾则先制+1并无视其护盾
//
// TODO（需新引擎能力，逐条留痕）：
//   604 黯玉咒言「威力随机50-150，连续使用每次+50，最高150」——需 per-skill 连用计数
//       （现成 9「连续使用每次威力增加{0}，最高威力{1}」可直接套用该模板，未接）；
//   571 陨灵命劫「4回合后造成500固伤，重复使用不叠加」——需延迟结算计时器；
//   1304 魂殒若梦「下N回合攻击忽略对手{1}%双防」——需伤害管线的"忽略双防%"阶段；
//   1779 命诞如虚「对手下N次切换时吸取下只出场精灵1/{1}体力」——需 EVENT_SWAP 载荷；
//   1702 命诞如虚「自身主动切换下场后令己方下只出战时恢复{0}体力」——需下场→下只登场回调。
#include <effects/lingchao_layers.h>

#include "common.h"

namespace {

// ⚠️ 与 soul_lib/lingchao_4599.cpp 同一注册键（effect_icon.effect_id）。
//    4599 是 monster id，不能作魂印键（2026-09-27 对齐，见 include/effects/lingchao_layers.h 同款教训）。
constexpr int kLingchaoMarkId = 2193;

// ── 1164 反转对手能力提升状态，若未满足条件或反转失败则消除对手能力提升状态和回合类效果 ──
// Args=[owner,target]。走真原语：反转（查免弱 STAT_DROP）；未反转 → 消除提升（查 STAT_CLEAR）
// + 断回合。"未满足或反转失败"两条件合一：只要没真反转就执行补偿分支。
EffectResult effect_1164_reverse_or_clear(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->stat_boost_reversal) {
        return EffectResult::kOk;
    }
    if (g_core->stat_boost_reversal(ctx, target) == StatReversalResult::REVERSED) {
        return EffectResult::kOk;   // 反转成功 → 补偿分支不执行
    }
    if (g_core->clear_stat_boosts) {
        g_core->clear_stat_boosts(ctx, target);
    }
    if (g_core->break_round_effects) {
        g_core->break_round_effects(ctx, target);
    }
    return EffectResult::kOk;
}

// ── 1686 先出手时造成的攻击伤害提升{0}%，若对手当前体力值高于自身则伤害额外提升{1}% ──
// Args=[owner,target,pct,extra]。写 ws.damage_add_pct（攻击伤害 AMP 阶段读取）。
EffectResult effect_1686_first_move_boost(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    const int extra = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (ctx->is_second_mover(owner)) {
        return EffectResult::kOk;   // 仅先出手
    }
    int total = pct;
    if (ctx->getPet(target).hp > ctx->getPet(owner).hp) {
        total += extra;   // 对手体力更高 → 额外提升
    }
    if (total > 0) {
        ctx->ws.damage_add_pct[owner] += total;
    }
    return EffectResult::kOk;
}

// ── 1581 出手时对手体力低于最大体力的1/{0}则造成的伤害提升{1}% ──
EffectResult effect_1581_low_hp_target_boost(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2] > 0 ? args.int_args[2] : 2;
    const int pct = (args.int_count >= 4) ? args.int_args[3] : 200;
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(target).hp * denom < max_hp_of(ctx, target)) {
        ctx->ws.damage_add_pct[owner] += pct;
    }
    return EffectResult::kOk;
}

// ── 933 未击败对手则己方下{0}次攻击技能必定打出致命一击 ──
// Args=[owner,target,counts]。次数型：计数存 plugin_storage，攻击命中时置 must_crit 并扣次。
EffectResult effect_933_next_attacks_crit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int counts = args.int_args[2] > 0 ? args.int_args[2] : 2;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int foe = 1 - owner;
    if (ctx->seerRobot[foe].elfPets[ctx->on_stage[foe]].hp <= 0) {
        return EffectResult::kOk;   // 已击败对手 → 不算"未击败"
    }
    ctx->plugin_storage[990933001 + owner] = counts;
    // ⚠️ 监听旗标键段必须与计数键段（990933001+side）完全错开：旧写法 990933002+owner
    //    与"side=1 的计数键"（990933001+1）碰撞——持有方在 side0 时旗标存 bool、
    //    对手 EVENT_HIT 一来就被 any_cast<int> 读 bool → bad_any_cast 崩溃
    //    （王·天决神威 35653 场景 330 首次暴露，2026-09-28 夜跑修复）。
    const int watch_key = 990933101 + owner;
    if (!ctx->plugin_storage.count(watch_key)) {
        ctx->plugin_storage[watch_key] = true;
        ctx->event_center_.register_watcher(
            EventType::EVENT_HIT, owner, ctx->roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.actor < 0 || ev.actor > 1) {
                    return;
                }
                const int side = ev.actor;
                auto it = c->plugin_storage.find(990933001 + side);
                if (it == c->plugin_storage.end()) {
                    return;
                }
                const int left = any_int_or(it->second);
                if (left <= 0) {
                    return;
                }
                if (c->ws.skill_type_view[side] == static_cast<int>(SkillType::Attribute)) {
                    return;   // "攻击技能"限定，属性技能不消耗
                }
                c->ws.must_crit[side] = true;
                c->plugin_storage[990933001 + side] = left - 1;
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 452 {0}合内自身所有攻击造成的伤害都将为自己恢复体力 ──
// Args=[owner,target,rounds]。窗口 = ROUND_START 回合类条目（可被断）；每次攻击后按**裸伤**回血。
EffectResult effect_452_window_tick(BattleContext* ctx, const EffectArgs& args) {
    (void)ctx;
    (void)args;
    return EffectResult::kOk;   // 窗口存在性凭证（判活由存储的起止回合承载）
}

EffectResult effect_452_attack_lifesteal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2] > 0 ? args.int_args[2] : 3;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    Effect wrapper(452, 0, owner, /*left_round=*/-1,
                   EffectArgs(std::vector<int>{owner, target}), &effect_452_window_tick);
    auto ce = std::make_unique<ContinuousEffect>(
        wrapper, State::BATTLE_ROUND_START, owner, rounds, ctx->roundCount);
    ce->source_id_ = 452;
    ce->scope_ = EffectScope::ON_STAGE;
    ctx->register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
    // 窗口起止（watcher 判活用）。
    // ⚠️ 键位必须按 owner*2 错开：连续序号 + owner 会跨界碰撞
    //（002+0 == 001+1），曾把 0 号方的注册守卫布尔写进 1 号方 rounds 的读键位，
    // watcher 里 any_cast<int>(bool) 抛 bad_any_cast 直接 abort（fuzz seed 446 首曝）。
    ctx->plugin_storage[990452001 + owner * 2] = ctx->round_effect_start_round(
        owner, rounds, EffectWindowKind::InRounds);
    ctx->plugin_storage[990452001 + owner * 2 + 1] = rounds;
    const int watch_key = 990452005 + owner;
    if (!ctx->plugin_storage.count(watch_key)) {
        ctx->plugin_storage[watch_key] = true;
        ctx->event_center_.register_watcher(
            EventType::EVENT_HIT, owner, ctx->roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.actor < 0 || ev.actor > 1 || !g_core || !g_core->heal_amount) {
                    return;
                }
                const int side = ev.actor;
                auto st = c->plugin_storage.find(990452001 + side * 2);
                auto rd = c->plugin_storage.find(990452001 + side * 2 + 1);
                if (st == c->plugin_storage.end() || rd == c->plugin_storage.end()) {
                    return;
                }
                // 指针形式 any_cast：类型不符返回 nullptr（对面效果的存储键、守卫布尔
                // 都可能落进同前缀键空间），绝不用会抛的取值形式。
                const int* start = std::any_cast<int>(&st->second);
                const int* rounds = std::any_cast<int>(&rd->second);
                if (start == nullptr || rounds == nullptr) {
                    return;
                }
                if (c->roundCount < *start || c->roundCount >= *start + *rounds) {
                    return;
                }
                if (c->ws.skill_type_view[side] == static_cast<int>(SkillType::Attribute)) {
                    return;   // 只认攻击伤害
                }
                const int raw = c->ws.raw_attack_damage[side];
                if (raw > 0) {
                    g_core->heal_amount(c, side, raw);
                }
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 712 {0}回合内将对手的{1}能力降为0点 ──
// Args=[owner,target,rounds,stat]。千魂之结原文"攻击、特攻、能力降为0点"——按"置零"语义
// （不是减 N）：每回合开始把对手的攻击/特攻正等级直接归零。
EffectResult effect_712_zero_stat_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    for (const int stat : {0 /*攻击*/, 2 /*特攻*/}) {
        const int lv = ctx->ability_level(target, stat);
        if (lv > 0) {
            g_core->stat_change(ctx, target, stat, -lv);
        }
    }
    return EffectResult::kOk;
}

EffectResult effect_712_zero_stat(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2] > 0 ? args.int_args[2] : 3;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    Effect wrapper(712, 0, owner, /*left_round=*/-1,
                   EffectArgs(std::vector<int>{owner, target}), &effect_712_zero_stat_tick);
    auto ce = std::make_unique<ContinuousEffect>(
        wrapper, State::BATTLE_ROUND_START, owner, rounds, ctx->roundCount);
    ce->source_id_ = 712;
    ce->scope_ = EffectScope::ON_STAGE;
    ctx->register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
    return EffectResult::kOk;
}

// ── 2106 为自身增加1层命起，然后若自身的命起高于魂落则使自身恢复最大体力的1/{0}
//    并造成等量百分比伤害 ──
// 层数走共享层（与魂印侧同源）；层和恒 4 → 命起+1 则魂落镜像 -1。
EffectResult effect_2106_add_mingqi(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2] > 0 ? args.int_args[2] : 1;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const int self_slot = lingchao_find_slot(ctx, owner, kLingchaoMarkId);
    if (self_slot < 0) {
        return EffectResult::kOk;   // 该宠不是灵巢（技能由 DB 挂载，兜底）
    }
    const int mingqi = lingchao_layers(ctx, owner, self_slot, kLingchaoMingqiKey);
    int new_mingqi = mingqi + 1;
    if (new_mingqi > kLingchaoLayerSum) {
        new_mingqi = kLingchaoLayerSum;
    }
    lingchao_set_layers(ctx, owner, self_slot, new_mingqi, kLingchaoLayerSum - new_mingqi);

    if (new_mingqi <= kLingchaoLayerSum - new_mingqi) {
        return EffectResult::kOk;   // 命起未高于魂落 → 无收益
    }
    if (!g_core || !g_core->heal_amount || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int amount = max_hp_of(ctx, owner) / denom;
    const int before = ctx->getPet(owner).hp;
    g_core->heal_amount(ctx, owner, amount);
    const int healed = ctx->getPet(owner).hp - before;   // 实际恢复量（吃封回血/上限）
    if (healed > 0) {
        g_core->deal_pink_damage(ctx, target, healed, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 2107 为自身增加1层魂落，然后若自身的魂落高于命起则令对手场下全部阵亡精灵消逝，
//    对手场下没有阵亡精灵时附加{0}点轮回系伤害 ──
EffectResult effect_2107_add_hunluo(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int points = args.int_args[2] > 0 ? args.int_args[2] : 200;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const int self_slot = lingchao_find_slot(ctx, owner, kLingchaoMarkId);
    if (self_slot < 0) {
        return EffectResult::kOk;
    }
    const int hunluo = lingchao_layers(ctx, owner, self_slot, kLingchaoHunluoKey);
    int new_hunluo = hunluo + 1;
    if (new_hunluo > kLingchaoLayerSum) {
        new_hunluo = kLingchaoLayerSum;
    }
    lingchao_set_layers(ctx, owner, self_slot, kLingchaoLayerSum - new_hunluo, new_hunluo);

    if (new_hunluo <= kLingchaoLayerSum - new_hunluo) {
        return EffectResult::kOk;   // 魂落未高于命起 → 无收益
    }
    if (!g_core) {
        return EffectResult::kOk;
    }
    int vanished = 0;
    if (g_core->vanish_spirit) {
        for (int slot = 0; slot < 6; ++slot) {
            if (slot == ctx->on_stage[target]) {
                continue;   // 场下限定
            }
            const ElfPet& pet = ctx->seerRobot[target].elfPets[slot];
            if (pet.hp <= 0 && !ctx->is_vanished(target, slot)
                && g_core->vanish_spirit(ctx, target, slot, owner) == VanishResult::VANISHED) {
                ++vanished;
            }
        }
    }
    if (vanished == 0 && g_core->deal_attribute_damage) {
        // 无阵亡可消逝 → 附加{0}点轮回系伤害（属性伤害：带系别、走红伤管线）。
        const int cycle_elem[2] = {11, 0};   // 轮回系（DB element 11）
        g_core->deal_attribute_damage(ctx, target, points, cycle_elem, owner);
    }
    return EffectResult::kOk;
}

// ── 1952 无视对手能力提升状态，无视成功则{0}%令对手{1} ──
// Args=[owner,target,pct,anomaly]。⚠️ v1：把对手正等级归零表达"无视"（真口径应只影响本次
// 伤害、不改状态——待伤害管线支持"忽略防御方提升"标志后改回）。
EffectResult effect_1952_ignore_boosts(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    bool had_boost = false;
    for (int stat = 0; stat < BattleContext::kAbilityLevelSlotCount; ++stat) {
        if (ctx->ability_level(target, stat) > 0) {
            had_boost = true;
            break;
        }
    }
    if (!had_boost) {
        return EffectResult::kOk;   // 对手无提升 → "无视成功"不成立 → 不触发后续
    }
    if (g_core->stat_change) {
        for (int stat = 0; stat < BattleContext::kAbilityLevelSlotCount; ++stat) {
            const int lv = ctx->ability_level(target, stat);
            if (lv > 0) {
                g_core->stat_change(ctx, target, stat, -lv);
            }
        }
    }
    if (g_core->apply_anomaly && is_valid_abnormal_status_id(anomaly) && roll_percent(pct)) {
        g_core->apply_anomaly(ctx, target, anomaly, random_anomaly_duration(), owner);
    }
    return EffectResult::kOk;
}

// ── 2079 对手处于护盾状态时先制+1并无视对手护盾效果 ──
// Args=[owner,target]。时点 = MOVE_RIGHT（先制判定）；"无视护盾"走 ws.ignore_shield。
EffectResult effect_2079_priority_vs_shield(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(target).shield_bank_.total() <= 0) {
        return EffectResult::kOk;   // 对手无护盾 → 不触发
    }
    ctx->ws.preemptive_level[owner] += 1;
    ctx->ws.ignore_shield[owner] = true;
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）──────────────────────────────────────────
// ⚠️ 2079 需要 MOVE_RIGHT 时点（先制判定）——已在 skills.cpp 的 effect_register_state 补 case。

    SKILL_EFFECT(1164, effect_1164_reverse_or_clear);        // 迴还：反转或消除对手提升+回合类
    SKILL_EFFECT(1686, effect_1686_first_move_boost);        // 迴还：先手增伤（对手体力高额外）
    SKILL_EFFECT(1581, effect_1581_low_hp_target_boost);     // 碎魂之爪：对手低血增伤
    SKILL_EFFECT(452, effect_452_attack_lifesteal);          // 凝魂之息：攻击伤害回血
    SKILL_EFFECT(933, effect_933_next_attacks_crit);         // 蒙尘古音：未击败→下N次必暴
    SKILL_EFFECT(712, effect_712_zero_stat);                 // 千魂之结：对手攻/特攻置零
    SKILL_EFFECT(2106, effect_2106_add_mingqi);              // 命诞如虚：命起+1 联动
    SKILL_EFFECT(2107, effect_2107_add_hunluo);              // 魂殒若梦：魂落+1 联动
    SKILL_EFFECT(1952, effect_1952_ignore_boosts);           // 无念归空净：无视对手提升→异常
    SKILL_EFFECT(2079, effect_2079_priority_vs_shield);      // 无念归空净：对手有护盾→先制+1
