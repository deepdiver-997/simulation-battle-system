#include <utils/sbs_rand.h>
// 帝皇之御 3512 技能组（2026-09-30；场景回归 test/scenario/scenario_781_diwang_3512.cpp）
//
// 落地的效果（effect_info 全文）：
//   1275 守御八方 31505（PP1，先制+2，必中）：
//       "消耗自身全部体力，使下只出场精灵前{0}次出手必定先手必定暴击" args=[2]
//   753  生命永恒 25861（PP1，必中，体力<1/2 先制+2=1117 已实现）：
//       "消耗自身全部体力，使对手{0}回合内使用属性技能失效（延续到对手所有精灵）" args=[5]
//   1276 救赎之羽 31506 尾段（首段 1187=100%疲惫已实现）：
//       "消耗自身全部体力，给对手附加消耗体力1/{0}等量的百分比伤害" args=[2]（31506=[100,7,2] 按
//       各效果 args_num 切片，本效果只拿尾参）
//   995  华光洗礼 31502 尾段（首段 1219 已实现）：
//       "消除对手能力提升状态，消除成功则吸取对手最大体力的1/{0}" args=[3]
//   1277 神谕涤罪曲 31507（PP1，必中，威力 0）args=[]：
//       "若体力大于对手，则消耗自身体力至残留1点HP给对手造成350-550点伤害；若体力小于对手，
//        则消耗自身全部体力给对手等同于消耗量的固定伤害并使对手瘫痪2回合"
//       （模板尾句"若对手受到致命伤害时残留1点体力"疑为截断残文，未单独建模——分支二本就
//        走死亡漏斗、对手残留体力类免死照常应答；待官方全文/实测翻案。）
//
// ★ 守御八方传承（次数型必先+必爆+命中修正，2026-09-30 用户实测定档）：
//   · **全队共享 2 次**（官方文本"下只出场精灵"，实测为全队保留 2 次机会——按实测档）；
//   · 消耗=真正出手（SELECT_SKILL 且未被控拦下）。嗑药（USE_MEDICINE）/切换/被控无法出手
//     **不消耗**（用后即耗家族"被控/切宠跳过主流程不经过结算→不消费"同款，§7.5）；
//   · 必先 tier=1：**真必先**（独立比较项），只是必先内部再分级——语料"必先只是高先制
//     等级"说的是部分 boss 特性的必先图标；pvp 真必先之间有优先级差，且**表姐自爆 >
//     无始源光**（用户 2026-09-30 实测；无始源光落地时不得高于本档）。魂印/特性必先最高
//     （opus 803656370681282613"表姐的必先在必先优先等级里面只能算是中下级别"）→ 本效果
//     取最低档；同级落回先制比较（引擎既有口径）；
//   · 必爆 = ws.must_crit（must_crit 单列通道，crit_rate==0 也能必暴）；
//   · 命中修正两层（用户实测：必闪词条无效、失明 55 开、身上有弱化仍可能 Miss）：
//       ① `ws.accuracy_fix_grant`（基础命中率钉 100 + 跳过必闪减避——**弱化负档照样乘**）；
//       ② **条件必中凭证**：仅当最终结算前无命中系弱化（命中等级非负、无命中率降档、
//          无混乱/易燃）才置 `ws.must_hit_grant`——凭证负责失明 55 开与"必 miss"类效果穿透。
//     用户原话推测"先修正基础命中率100%再给条件必中凭证"与本落点一致。
//   · 消费点 AFTER_ACTION（行动方门 + 本轮武装标记门——自爆当轮不消费）。
//
// ★ 自爆契约（2213/1212 同款）：调用方先把体力打到 0 → defeat_pet(HP_CONSUME)——
//   HP_CONSUME 穿残留免死（sees_hp_consume=false 的拦截器不被问询）→ "自爆不触发免死"。
//
// ★ 生命永恒封属：seal_skill(hit_invalid=true) = SEAL_ATTRIBUTE_HIT（属性技能照常命中、
//   效果不注册、不走无效补偿——"属性技能命中效果失效"，与普通封属 SEAL_ATTRIBUTE 分立）；
//   EffectScope::TEAM —— 延续到对手所有精灵 + **自爆死亡后封属仍在**（2213 同款：
//   条目不随施放方死亡/换宠清）；施放方已死 → 断回合无锚可断 → "无法被断"。
#include "common.h"

namespace {

// plugin_storage 段 3512xxxx（grep 全库无撞；int 值 + std::any）。
constexpr int kVoucherChargesKey = 35120100;   // +side：守御八方传承剩余次数
constexpr int kVoucherArmedKey = 35120101;     // +side：本轮已武装（roundCount，防自爆当轮误消费）
constexpr int kVoucherWiredKey = 35120102;     // 接线标记（战斗级，一次）

// ── AFTER_ACTION 桶的当前行动方（hamo_3809_longhun 同款）────────────────────
int bucket_acting_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

// ── 传承武装（MOVE_RIGHT 桶：ws 先手权 memset 之后、出手之前）────────────────
EffectResult diwang_voucher_move_right(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1275, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 1, 1275, args, "需要 [owner]");
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    auto charges = ctx->plugin_storage.find(kVoucherChargesKey + owner);
    if (charges == ctx->plugin_storage.end()
        || any_int_or(charges->second) <= 0) {
        return EffectResult::kOk;   // 次数耗尽/未授予
    }
    if (!used_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;   // 嗑药/切换回合：不武装也不消耗
    }
    ctx->ws.guaranteed_first[owner] = 1;      // 必先 tier1（中下级别，见文件头）
    ctx->ws.must_crit[owner] = true;          // 必定暴击
    ctx->ws.accuracy_fix_grant[owner] = true; // 基础命中率钉 100 + 必闪词条无效
    // 条件必中凭证：最终结算前无命中系弱化才授予（有弱化 → 走 roll、负档可 Miss）。
    const int hit_level = ctx->ws.view_levels[owner][kAbilityLevelIndexHit]
                        + ctx->ws.hit_level_extra[owner];
    bool clean = hit_level >= 0 && ctx->ws.hit_rate_mod[owner] >= 1.0f;
    if (clean && g_core && g_core->has_active_anomaly) {
        if (g_core->has_active_anomaly(ctx, owner, static_cast<int>(AbnormalStatusId::Confusion))
            || g_core->has_active_anomaly(ctx, owner,
                                          static_cast<int>(AbnormalStatusId::Flammable))) {
            clean = false;
        }
    }
    if (clean) {
        ctx->ws.must_hit_grant[owner] = true;
    }
    ctx->plugin_storage[kVoucherArmedKey + owner] = ctx->roundCount;
    return EffectResult::kOk;
}

// ── 传承消费（FIRST/SECOND AFTER_ACTION 双桶 + 行动方门）────────────────────
EffectResult diwang_voucher_after_action(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1275, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 1, 1275, args, "需要 [owner]");
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || bucket_acting_side(ctx) != owner) {
        return EffectResult::kOk;
    }
    auto armed = ctx->plugin_storage.find(kVoucherArmedKey + owner);
    if (armed == ctx->plugin_storage.end()
        || any_int_or(armed->second) != ctx->roundCount) {
        return EffectResult::kOk;   // 本轮没武装（自爆当轮/嗑药轮）→ 不消费
    }
    ctx->plugin_storage.erase(armed);
    if (!used_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;   // 被控无法出手（roundChoice≠SELECT_SKILL）→ 不消耗
    }
    auto charges = ctx->plugin_storage.find(kVoucherChargesKey + owner);
    if (charges != ctx->plugin_storage.end()) {
        const int left = any_int_or(charges->second);
        if (left > 0) {
            charges->second = left - 1;
        }
    }
    return EffectResult::kOk;
}

// ── 接线（幂等）：传承的 MOVE_RIGHT / AFTER_ACTION tick ────────────────────
//
// ⚠️ 生命周期（2026-09-30 场景 781 实测教训）：技能桶条目锚定**注册者的 epoch**——
// 表姐 R1 自爆死亡 → 她名下的常驻 tick 随下场作废，R2 必先必爆全体哑火。传承是
// **全队**资产，必须活过施放者死亡：
//   · tick 一律在**己方登场时点**重挂（挂在刚登场、还活着的精灵名下；同源注册覆盖，
//     幂等），ENTER_STAGE watcher（TEAM 域，撑过死亡换宠——1212/2213 同款）负责重挂；
//   · ensure_voucher_wiring 里的首次挂载只服务"当回合用完自爆、当回合就消费"的窗口，
//     真正承接后续回合的是登场重挂。
void register_voucher_ticks(BattleContext* ctx, int owner) {
    {
        Effect e(1275, 0, owner, /*duration=*/-1, EffectArgs(std::vector<int>{owner}),
                 &diwang_voucher_move_right);
        auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_MOVE_RIGHT, owner,
                                                     -1, ctx->roundCount);
        ce->source_id_ = 1275;
        ctx->register_skill_effect(State::BATTLE_FIRST_MOVE_RIGHT, owner, std::move(ce));
    }
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(1275, 0, owner, /*duration=*/-1, EffectArgs(std::vector<int>{owner}),
                 &diwang_voucher_after_action);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, -1, ctx->roundCount);
        ce->source_id_ = 1275;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
}

void ensure_voucher_wiring(BattleContext* ctx, int owner) {
    if (!ctx || ctx->plugin_storage.count(kVoucherWiredKey + owner)) {
        return;
    }
    ctx->plugin_storage.emplace(kVoucherWiredKey + owner, 1);
    // 己方每次登场（死亡换宠/主动切换）→ 重挂传承 tick（挂到刚登场的活精灵名下）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (c && ev.actor == owner) {
                register_voucher_ticks(c, owner);
            }
        },
        WatcherScope::TEAM);
    register_voucher_ticks(ctx, owner);   // 兜底：当回合的 AFTER_ACTION 消费
}

// ── 自爆公共尾段：体力打到 0 → HP_CONSUME 死亡漏斗（穿残留免死）──────────────
void self_destruct_hp_consume(BattleContext* ctx, int owner, int actor) {
    if (!g_core || !g_core->defeat_pet) {
        return;
    }
    const int slot = ctx->on_stage[owner];
    ElfPet& self = ctx->seerRobot[owner].elfPets[slot];
    if (self.hp <= 0) {
        return;   // 已死（反弹/反伤等）——自爆无从谈起
    }
    self.hp = 0;
    g_core->defeat_pet(ctx, owner, slot, actor, DefeatCause::HP_CONSUME);
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 1275 守御八方 —— 消耗自身全部体力，使下只出场精灵前{0}次出手必定先手必定暴击
//
// ★ 自爆 = **印记形态**（2026-09-30 哈莫链复刻，用户定稿机制链）：清 0（体力修正
//   通道，无预死亡信号 → 免死不触发——"自爆不触发免死"）+ 挂"待决死亡印记"
//   （plugin_storage 35120208，与 soul_lib/diwang_3512.cpp 共享段），印记在
//   SECOND_MOVER_DEATH 死亡检查时点收尾（soul 文件节点③）——"哈莫③固伤救场 →
//   后手照常行动 → 印记收尾才死"的复刻载体。753 生命永恒保持即时 HP_CONSUME
//   形态（不在复刻链上）。
//═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1275_shouyu_bafang(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1275, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 3, 1275, args, "需要 [owner, target, charges]");
    const int owner = args.int_args[0];
    const int charges = args.int_args[2];
    if (owner < 0 || owner > 1 || charges <= 0) {
        return EffectResult::kOk;
    }
    ensure_voucher_wiring(ctx, owner);
    ctx->plugin_storage[kVoucherChargesKey + owner] = charges;
    const int slot = ctx->on_stage[owner];
    ElfPet& self = ctx->seerRobot[owner].elfPets[slot];
    if (self.hp > 0) {
        self.hp = 0;   // 消耗自身全部体力（体力修正通道——不发预死亡信号）
        // 待决死亡印记：SECOND_MOVER_DEATH 死亡检查时点收尾（soul_lib 节点③）。
        ctx->plugin_storage[35120208 + owner] = 1;
    }
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 753 生命永恒 —— 消耗自身全部体力，使对手{0}回合内使用属性技能失效（延续到对手所有精灵）
//═══════════════════════════════════════════════════════════════════════════
EffectResult effect_753_shengming_yongheng(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 753, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 3, 753, args, "需要 [owner, target, rounds]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->seal_skill) {
        // 命中效果失效变体（SEAL_ATTRIBUTE_HIT）：属性技能照常命中、效果不注册、无补偿。
        // TEAM 域：延续到对手所有精灵；自爆死亡后条目仍在（2213 同款），施放方已死
        // → 断回合无锚可断（"无法被断"）。
        g_core->seal_skill(ctx, /*source=*/owner, /*target=*/target,
                           /*effect_id=*/753, /*attribute=*/true, /*attack=*/false,
                           /*count=*/0, /*duration_rounds=*/rounds, /*penetrable=*/true,
                           /*source_slot=*/-1, EffectScope::TEAM, /*hit_invalid=*/true,
                           /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    }
    self_destruct_hp_consume(ctx, owner, target);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1276 救赎之羽尾段 —— 消耗自身全部体力，给对手附加消耗体力1/{0}等量的百分比伤害
//═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1276_soulwing(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1276, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 3, 1276, args, "需要 [owner, target, denom]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    const int slot = ctx->on_stage[owner];
    ElfPet& self = ctx->seerRobot[owner].elfPets[slot];
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    // 消耗值在自爆前捕获；粉伤（PERCENT_VALUE=具体值）→ 自爆死亡收尾。
    const int consumed = self.hp;
    self.hp = 0;
    if (g_core && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, target, consumed / denom, DamageKind::PERCENT_VALUE, owner);
    }
    if (g_core && g_core->defeat_pet) {
        g_core->defeat_pet(ctx, owner, slot, target, DefeatCause::HP_CONSUME);
    }
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 995 华光洗礼尾段 —— 消除对手能力提升状态，消除成功则吸取对手最大体力的1/{0}
//═══════════════════════════════════════════════════════════════════════════
EffectResult effect_995_huaguang_xili(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 995, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 3, 995, args, "需要 [owner, target, denom]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || denom <= 0 || !g_core) {
        return EffectResult::kOk;
    }
    if (!g_core->clear_stat_boosts || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    const int cleared = g_core->clear_stat_boosts(ctx, target);
    if (cleared > 0) {
        g_core->drain_hp(ctx, /*actor=*/owner, target, denom);   // 目标掉 max_hp/{denom}，自身回等量
    }
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1277 神谕涤罪曲 —— 双分支自爆（args=[]，数值全在模板里）
//═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1277_shenyu_dizuiqu(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1277, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 2, 1277, args, "需要 [owner, target]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || !g_core) {
        return EffectResult::kOk;
    }
    const int slot = ctx->on_stage[owner];
    ElfPet& self = ctx->seerRobot[owner].elfPets[slot];
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& opp = ctx->seerRobot[target].elfPets[ctx->on_stage[target]];
    if (self.hp > opp.hp) {
        // 体力大于对手：消耗至残留 1 点 HP（不是死亡，不走漏斗），造成 350~550 点伤害。
        self.hp = 1;
        if (g_core->fixed_damage) {
            g_core->fixed_damage(ctx, target, 350 + ctx->rng().next() % 201);
        }
        return EffectResult::kOk;
    }
    // 体力小于对手：消耗全部体力 → 等量固定伤害 + 瘫痪 2 回合 → 自爆收尾。
    const int consumed = self.hp;
    self.hp = 0;
    if (g_core->fixed_damage) {
        g_core->fixed_damage(ctx, target, consumed);
    }
    if (g_core->apply_anomaly_checked) {
        g_core->apply_anomaly_checked(ctx, target, static_cast<int>(AbnormalStatusId::Paralysis),
                                      /*duration_rounds=*/2, owner, /*chance_pct=*/100,
                                      ChanceSource::Skill);
    }
    if (g_core->defeat_pet) {
        g_core->defeat_pet(ctx, owner, slot, target, DefeatCause::HP_CONSUME);
    }
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册）──
SKILL_EFFECT(1275, effect_1275_shouyu_bafang)
SKILL_EFFECT(753, effect_753_shengming_yongheng)
SKILL_EFFECT(1276, effect_1276_soulwing)
SKILL_EFFECT(995, effect_995_huaguang_xili)
SKILL_EFFECT(1277, effect_1277_shenyu_dizuiqu)
