// 克雷弗德 4644 魂印 —— 德莱赫默伴生构造系统
//
// 官方魂印（4399 图鉴 993768，2026-09-20 查）：
//   克雷弗德在场期间德莱赫默与其一同战斗，技能中的构造效果每次在场首次生效时德莱赫默
//   获得对应构造；德莱赫默初始守备，构造数不低于2时转变为激发状态，构造数下场重置。
//   回合开始时若守备：每空缺一种构造受到的攻击伤害-10%，回合结束恢复所有攻击技能1PP；
//   若激发：每拥有1种构造造成的攻击伤害+15%，攻击技能先制+1。
//   克雷弗德选择了技能且无法行动的回合，德莱赫默在行动阶段结束后额外行动：
//   激发=核威强袭（电/机械/电机械中克制最高的系别 300 点伤害）；
//   守备=激鸣雷封（100%麻痹对手+克雷弗德300护盾）。（boss无效——PvP 模拟忽略）
//
// 关键口径（用户 2026-09-20 拍板）：
//   ★ 守备减伤**加法**叠加（10%×空缺数，与套装减伤同桶可叠到 100% 减伤）；
//     激发增伤同理加法（15%×构造数）。"攻击技能先制+1"= **每构造 +1**（最高+4）。
//   ★ 回合开始时**快照**守备/激发态：本回合中途点亮第2构造，本回合仍按守备结算，
//     下回合开始才享受激发（构造数在回合开始后、自身出手前不会变化，先制用快照数）。
//   ★ 构造点亮由技能侧 SKILL_EFFECT（HIT 分支=命中且生效）写发布槽，本侧只读；
//     每次登场（ENTER_EXIT_STAGE）清零位图 + 守备态 = "构造数下场重置"。
//   ★ 被控回合的德莱赫默额外行动：ACTION_START 桶（先于 should_skip 判定跑）里
//     "点了技能 + 活着 + 控制类异常"→ declare_extra_action；嗑药/换宠（非 SELECT_SKILL）
//     不触发。先手/后手各自在 FIRST/SECOND_EXTRA_ACTION 时点行动（先手权天然有序）。
//     控制扫描跳过狂信(41)——条件禁行动（信仰对象引用内核私有，插件不可达）。
//   ★ 超越式(37358) 构造<2 的先制-1：MOVE_RIGHT 桶（memset 后、比较前）按**选择期**结算。
//   ★ 超越式 构造≥2 的"无视自身下降/对手提升"：SKILL_EFFECT 钳制双方视图等级
//     （自身负→0、对手正→0），AFTER_ACTION 还原——伤害管线（ATTACK_DAMAGE）恰好夹在中间。
//   ★ 构造II 的"对手下次攻击-50%"：ws 加算减伤桶按**防守方**索引 → 写在**自己**桶里
//     （对手打我时我就是 defender）；撤除挂 EVENT_TAKE_DAMAGE（target=自己、actor=对手、
//     state=ATTACK_DAMAGE——只有攻击管线的红伤消费，属性直伤/粉伤/额外行动不消费，
//     用户口径"不要对属性技能响应消耗"）。
//   ★ 激鸣雷封的麻痹走 apply_anomaly 标准原语（弹控/抗性/转化全链响应）。
#include "common.h"
#include "../crawford_runtime.h"

namespace {

// ⚠️ 注册键 = effect_icon.effect_id = 2231（装配层 SoulMark.id 按 effect_id 查程序表）。
//    4644 是 monster id，只用于命名与注释——错键会让程序在真实对局挂不上（2026-09-27 修复）。
constexpr int kMark = 2231;
constexpr int kFifthSkillId = 37358;   // 超越式·变革律
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);
constexpr int kElectric = 5;           // skill_types：电
constexpr int kMechanical = 6;         // 机械
constexpr int kDeleermoShield = 300;   // 激鸣雷封护盾值
constexpr int kShieldSlotPct = 10;     // 守备减伤：每空缺构造 10%
constexpr int kExcitedPct = 15;        // 激发增伤：每构造 15%

// ── 本地助手（moves_lib/common.h 的同名逻辑；soul 侧不共享那个头，就地复刻）──
// 本回合该 side 是否真的点了技能（SELECT_SKILL=1；嗑药/换宠/未提交不算）。
bool chose_skill_this_round(const BattleContext* ctx, int side) {
    return ctx && side >= 0 && side <= 1 && ctx->roundChoice[side][0] == 1;
}

// 本回合选中技能是否为攻击技能（物理/特殊）。
bool selected_skill_is_attack(BattleContext* ctx, int side) {
    if (!chose_skill_this_round(ctx, side)) {
        return false;
    }
    const int idx = ctx->roundChoice[side][1];
    if (idx < 0 || idx >= static_cast<int>(ctx->getPet(side).skills.size())) {
        return false;
    }
    return ctx->getPet(side).skills[static_cast<std::size_t>(idx)].type != SkillType::Attribute;
}

// 宿主（克雷弗德）当前是否在场。
bool host_on_stage(BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    const int slot = ctx->on_stage[side];
    return slot >= 0 && ctx->seerRobot[side].elfPets[slot].soulMark.id == kMark;
}

// 控制类异常禁行动（has_blocking_control_status 的插件侧复刻）。
// 狂信(41) 是条件禁行动（信仰对象内核私有）→ 插件侧按"不挡"处理。
// 异常槽查询走 CoreApi（has_active_abnormal_status 是核心符号，插件不可链接）。
bool blocked_by_control(BattleContext* ctx, int side) {
    if (!g_core || !g_core->has_active_anomaly) {
        return false;
    }
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (id == static_cast<int>(AbnormalStatusId::Fanaticism)) {
            continue;
        }
        if (g_core->has_active_anomaly(ctx, side, id)
            && is_control_abnormal_status(static_cast<AbnormalStatusId>(id))) {
            return true;
        }
    }
    return false;
}

// 抢第一个空的加算减伤槽（chaos_sorensen 同款：多来源各占一槽，ws 每回合 reset 重挑）。
int grab_reduce_slot(BattleContext* ctx, int side, int pct) {
    int(&slots)[4] = ctx->ws.damage_reduce_add[side];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = pct;
            return i;
        }
    }
    return -1;
}

// 撤销"对手下次攻击-50%"：清旗 + 清记录的减伤桶槽（消费与离场撤销同一动作）。
// 口径（用户 2026-09-20）：这刀减伤**绑定克雷自身**——主动切换/阵亡离场即失效，
// 再上场不恢复（要重新用限/超越式获得）；已 pending 时重复获得只覆盖不叠加（moves 侧幂等）。
void revoke_opp_reduce(BattleContext* ctx, int side) {
    if (!crawford_pub_opp_reduce(ctx->plugin_storage, side)) {
        return;
    }
    crawford_pub_opp_reduce(ctx->plugin_storage, side, false);
    const auto slot_it = ctx->plugin_storage.find(kCrawfordPubOppReduceBase + 100 + side);
    if (slot_it != ctx->plugin_storage.end()) {
        const int slot = any_int_or(slot_it->second);
        if (slot >= 0 && slot < 4) {
            ctx->ws.damage_reduce_add[side][slot] = 0;
        }
    }
}

// ── 登场：只补挂持久监听 ──
// 构造/快照状态存 ElfPet::on_stage_storage——换宠由引擎清空（battleFsm perform_switch
// 的 old_pet.on_stage_storage.clear()），"构造数下场重置"不再需要手动复位；
// "-50%"待响应旗在 plugin_storage：**绑定克雷自身**，离场（主动切换/阵亡）即撤销。
EffectResult crawford_enter(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !host_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    // 三个持久监听只挂一次（句柄在 soulmark_storage，跨在场保留）。
    if (crawford_take_damage_watcher(ctx->getPet(side)) == 0) {
        crawford_set_take_damage_watcher(ctx->getPet(side), ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, side, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [side](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.target != side || ev.actor != 1 - side) {
                    return;
                }
                // 只有**攻击管线**的红伤消费"下次攻击-50%"（属性直伤/粉伤/真伤/额外行动不消费——
                // 用户口径：次数减伤不响应属性技能）。
                const int st = ev.state;
                if (st != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                    && st != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                    return;
                }
                revoke_opp_reduce(c, side);
            },
            WatcherScope::TEAM));
        // 主动切换 → 减伤失效（EVENT_SWAP 只在主动中切 emit，死亡换宠不发）。
        ctx->event_center_.register_watcher(
            EventType::EVENT_SWAP, side, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [side](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.actor != side) {
                    return;
                }
                revoke_opp_reduce(c, side);
            },
            WatcherScope::TEAM);
        // 阵亡 → 减速失效（EVENT_DEATH emit 于换宠前：on_stage 仍是自己且 hp=0；
        // 换宠后 drain 时 hp>0 的守卫挡掉队友/后场死亡误清）。
        ctx->event_center_.register_watcher(
            EventType::EVENT_DEATH, side, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [side](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.target != side || c->getPet(side).hp > 0) {
                    return;
                }
                revoke_opp_reduce(c, side);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 回合开始：快照守备/激发 + 双向减伤/增伤 + "-50%"跨回合重申 ──
EffectResult crawford_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !host_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    CrawfordRuntime& rt = crawford_rt(ctx->getPet(side));
    rt.snapshot_count = crawford_count_constructs(crawford_constructs(ctx->getPet(side)));
    rt.excited_snapshot = rt.snapshot_count >= 2;
    if (rt.excited_snapshot) {
        // 激发：每构造 +15%（通用增伤·加算桶，AMP 阶段求和）。
        ctx->ws.damage_add_pct[side] += static_cast<float>(kExcitedPct * rt.snapshot_count);
    } else {
        // 守备：每空缺一种构造 -10%（加算减伤桶，可与套装减伤叠到 100%）。
        grab_reduce_slot(ctx, side, kShieldSlotPct * (4 - rt.snapshot_count));
        rt.pp_restore_pending = true;
    }
    // 构造II 的"-50%"跨回合重申（ws 每回合 reset，未消费就续写自己的防守桶）。
    if (crawford_pub_opp_reduce(ctx->plugin_storage, side)) {
        const int slot = grab_reduce_slot(ctx, side, 50);
        ctx->plugin_storage[kCrawfordPubOppReduceBase + 100 + side] = slot;
    }
    return EffectResult::kOk;
}

// ── MOVE_RIGHT：激发"攻击技能先制+每构造1" + 超越式构造<2 的先制-1 ──
EffectResult crawford_move_right(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !host_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    CrawfordRuntime& rt = crawford_rt(ctx->getPet(side));
    if (rt.excited_snapshot && selected_skill_is_attack(ctx, side)) {
        ctx->ws.preemptive_level[side] += rt.snapshot_count;
    }
    // 超越式·变革律：德莱赫默构造数低于 2 → 先制-1（选择期口径）。
    if (chose_skill_this_round(ctx, side)) {
        const int idx = ctx->roundChoice[side][1];
        if (idx >= 0 && idx < static_cast<int>(ctx->getPet(side).skills.size())
            && ctx->getPet(side).skills[static_cast<std::size_t>(idx)].id == kFifthSkillId) {
            const int count =
                crawford_count_constructs(crawford_constructs(ctx->getPet(side)));
            if (count < 2) {
                ctx->ws.preemptive_level[side] -= 1;
            }
        }
    }
    return EffectResult::kOk;
}

// ── SKILL_EFFECT：超越式 构造≥2 → 无视自身下降与对手提升（伤害视图钳制）──
EffectResult crawford_skill_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !host_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[side][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[side][1];
    if (idx < 0 || idx >= static_cast<int>(ctx->getPet(side).skills.size())
        || ctx->getPet(side).skills[static_cast<std::size_t>(idx)].id != kFifthSkillId) {
        return EffectResult::kOk;
    }
    const int count =
        crawford_count_constructs(crawford_constructs(ctx->getPet(side)));
    if (count < 2) {
        return EffectResult::kOk;   // <2：先制-1 + 吸取（技能侧 2155 管），不动视图
    }
    CrawfordRuntime& rt = crawford_rt(ctx->getPet(side));
    if (rt.view_clamped) {
        return EffectResult::kOk;
    }
    rt.view_clamped = true;
    for (int i = 0; i < 6; ++i) {
        rt.clamped_self[i] = ctx->ws.view_levels[side][i];
        rt.clamped_opp[i] = ctx->ws.view_levels[1 - side][i];
        ctx->ws.view_levels[side][i] = rt.clamped_self[i] < 0 ? 0 : rt.clamped_self[i];
        ctx->ws.view_levels[1 - side][i] =
            rt.clamped_opp[i] > 0 ? 0 : rt.clamped_opp[i];
    }
    return EffectResult::kOk;
}

// ── AFTER_ACTION：还原钳制的视图等级 ──
EffectResult crawford_after_action(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    CrawfordRuntime& rt = crawford_rt(ctx->getPet(side));
    if (!rt.view_clamped) {
        return EffectResult::kOk;
    }
    rt.view_clamped = false;
    for (int i = 0; i < 6; ++i) {
        ctx->ws.view_levels[side][i] = rt.clamped_self[i];
        ctx->ws.view_levels[1 - side][i] = rt.clamped_opp[i];
    }
    return EffectResult::kOk;
}

// ── ACTION_START：被控回合声明德莱赫默额外行动（桶先于 should_skip 判定跑）──
EffectResult crawford_action_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !host_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    // "选择了技能且无法行动"：嗑药/换宠（非 SELECT_SKILL）与阵亡不触发。
    if (!chose_skill_this_round(ctx, side) || ctx->getPet(side).hp <= 0) {
        return EffectResult::kOk;
    }
    if (!blocked_by_control(ctx, side)) {
        return EffectResult::kOk;
    }
    ctx->declare_extra_action(side);
    return EffectResult::kOk;
}

// ── EXTRA_ACTION：德莱赫默按当前状态行动 ──
// 激发（构造数≥2，按**实时**数——转变即时）→ 核威强袭；
// 守备 → 激鸣雷封（100%麻痹 + 300 护盾）。
void deleermo_act(BattleContext* ctx, int side) {
    if (!host_on_stage(ctx, side)) {
        return;
    }
    const int count =
        crawford_count_constructs(crawford_constructs(ctx->getPet(side)));
    ElfPet& opp = ctx->seerRobot[1 - side].elfPets[ctx->on_stage[1 - side]];
    if (count >= 2) {
        // 核威强袭：电/机械/电机械中对对手克制倍数最高的系别，300 点属性伤害
        // （deal_attribute_damage 内部按 点数×克制倍数 结算，防方读视图系别）。
        if (!g_core || !g_core->deal_attribute_damage) {
            return;
        }
        // ⚠️ 克制矩阵只有叶子属性（types_relation 无组合 id 行）——"电机械"必须以
        // 分量对 {5,6} 参与 2v2 查表（calculateRestraintMultiples 按位相乘），组合 id
        // 122 作叶子查恒 1.0。
        const int candidates[3][2] = {{kElectric, 0}, {kMechanical, 0}, {kElectric, kMechanical}};
        double best_mult = -1.0;
        int best_elem[2] = {kElectric, 0};
        for (const auto& cand : candidates) {
            const double m = g_core->restraint_multiplier
                                 ? g_core->restraint_multiplier(cand,
                                                                ctx->ws.view_elementalAttributes[1 - side])
                                 : 1.0;
            if (m > best_mult) {
                best_mult = m;
                best_elem[0] = cand[0];
                best_elem[1] = cand[1];
            }
        }
        g_core->deal_attribute_damage(ctx, 1 - side, /*points=*/300, best_elem, side);
        (void)opp;
        return;
    }
    // 守备：激鸣雷封。
    if (g_core && g_core->apply_anomaly && opp.hp > 0) {
        g_core->apply_anomaly(ctx, 1 - side, kParalysis, /*duration=*/-1, side);
    }
    ctx->getPet(side).shield_bank_.add(ShieldBank::kSoulShieldPriority, kDeleermoShield,
                                       /*source_id=*/kMark, /*duration=*/0);
}

EffectResult crawford_extra_first(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    deleermo_act(ctx, side);
    return EffectResult::kOk;
}

EffectResult crawford_extra_second(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    deleermo_act(ctx, side);
    return EffectResult::kOk;
}

// ── 回合结束：守备快照回合 → 恢复所有攻击技能 1 PP ──
EffectResult crawford_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !host_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    CrawfordRuntime& rt = crawford_rt(ctx->getPet(side));
    if (!rt.pp_restore_pending || rt.excited_snapshot) {
        return EffectResult::kOk;
    }
    rt.pp_restore_pending = false;
    ElfPet& host = ctx->getPet(side);
    for (Skills& s : host.skills) {
        if (s.type == SkillType::Attribute) {
            continue;   // "所有攻击技能"（物理/特殊）
        }
        if (s.pp >= 0 && s.pp < s.maxPP) {
            s.pp += 1;
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(kMark,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &crawford_enter,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_START, &crawford_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &crawford_move_right,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT, &crawford_skill_effect,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT, &crawford_skill_effect,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_AFTER_ACTION, &crawford_after_action,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_AFTER_ACTION, &crawford_after_action,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_ACTION_START, &crawford_action_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_ACTION_START, &crawford_action_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_EXTRA_ACTION, &crawford_extra_first,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_EXTRA_ACTION, &crawford_extra_second,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_END, &crawford_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});
