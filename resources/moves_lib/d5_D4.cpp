// D4 · 瀚宇星皇 3291 第五技「万皇宗魄决 30381」缺失效果 1098（夜跑 night-2，2026-09-28）
//
// 1098 官方模板（effect_info，args_num=0，全部数字写死在文本里）：
//   "致命一击率提升20%，每次使用增加20%，最高100%；打出致命一击后令自身下2次技能触发的
//    星皇之怒威力不再减少;技能结束后对手体力值高于0，则50%进行一次额外行动以触发星皇之怒
//   （星皇之怒：下2次自身使用的攻击技能先制+2）"
// 4399 图鉴（万皇宗魄决）："必中；初始致命一击率为20%，每次使用提高20%，最高100%；
//   触发致命一击后自身下2次攻击技能触发连击效果时威力不再下降；（星皇之怒：自身下2次攻击先制+2）"
//
// 落地（星皇之怒 = 本 kit 1094~1098 共享骨架：额外行动触发、载荷逐技定义，见
// 《开工-额外行动与空元之诗.md》§2.4；额外行动机制 2026-09-18 已落地）：
//   ① 暴击梯度：30381 自带 crit_rate=0 → 唯一引爆通道是 ws.crit_rate_add（加算百分点，
//      "add 是引爆效果，base=0 照样能暴"）。梯度计数按"已用次数"存 plugin_storage，
//      MOVE_RIGHT 常驻节点在"本回合所选技==30381"时写 min((uses+1)*20, 100)——
//      ⚠️ 已知偏差：暴击掷点在 query_usage ①.5（先于 SKILL_EFFECT），本技能效果链没有
//      更早的武装点 → **第 1 次使用按 0% 掷**（官方 20%），第 2 次起与官方一致
//      （40%/60%/…/100%）。保守实现 + 此处记档待拍板。
//   ② 触发：SKILL_EFFECT 每用挂 rounds=1 的 AFTER_ACTION_END 节点（FIRST/SECOND 两侧，
//      只活在用技当回合）——"技能结束后对手体力值高于0"在伤害落地后判，50% 过则
//      declare_extra_action(owner)。技能被无效/miss 时 SKILL_EFFECT 不跑 → 不挂不触发。
//   ③ 载荷（星皇之怒：下2次自身使用的攻击技能先制+2）：EXTRA_ACTION 节点（FIRST/SECOND
//      两侧）+2 层 charges；MOVE_RIGHT 常驻节点消费——所选技是攻击技则 preemptive+2、
//      耗 1 层（应用即耗，miss/被无效不回补——先制加成在时点序上必然先于命中判定）。
//      属性技不耗层（"攻击技能"限定，与 481/681 同判据 selected_skill_is_attack）。
//   ④ ⛔ 未做（跳过记档）："打出致命一击后…星皇之怒威力不再减少"——压制的是星皇之怒
//      **本体**（以技能 50% 威力额外攻击，见《魂印机制设计与精灵表现档案》§4.3）的威力
//      折半，而本体属魂印 903 线且用户明确往后推（同上开工文档 §2.4），当前无行为载体；
//      4399"连击效果威力不再下降"在本引擎亦无对应机制（effect 9 是连用威力**递增**）。
//      待星皇之怒本体落地后回补。
//
// 场景回归：test/scenario/scenario_200_hanyu_3291_1098.cpp。

#include "common.h"

namespace {

constexpr int kSkillId = 30381;          // 万皇宗魄决
constexpr int kCritStep = 20;            // 每次使用 +20%
constexpr int kCritCap = 100;            // 最高 100%
constexpr int kTriggerPct = 50;          // 额外行动触发概率
constexpr int kWrathCharges = 2;         // 星皇之怒：下 2 次
constexpr int kPriorityBonus = 2;        // 先制 +2
constexpr int kBattleLongRounds = 999;   // 常驻节点的"战斗长"窗口

// plugin_storage 键（owner 侧各一份；9901098xx 段全库无冲突）
constexpr int kUsesKeyBase = 990109810;     // 万皇宗魄决已用次数（暴击梯度）
constexpr int kChargesKeyBase = 990109820;  // 星皇之怒载荷剩余层数
constexpr int kArmedKeyBase = 990109830;    // 常驻节点已注册旗标

// 辅助节点的注册 id：效果桶按 (source_id<<32)|effect_id **同源去重**（timed_effect_key），
// 同一 state 上两条节点若同 id 会互相顶替 → 各发一个伪 id（10^9 段，非官方效果 id；
// 只作桶键与 effect_window_kind 查询（查无 → 默认 InRounds），处理函数不依赖它）。
constexpr int kNodeCritArm = 10980301;      // MOVE_RIGHT：暴击梯度武装
constexpr int kNodePriority = 10980302;     // MOVE_RIGHT：先制消费
constexpr int kNodeTriggerFirst = 10980101; // AFTER_ACTION_END 先手侧：触发判定
constexpr int kNodeTriggerSecond = 10980102;// AFTER_ACTION_END 后手侧
constexpr int kNodePayloadFirst = 10980201; // EXTRA_ACTION 先手侧：载荷
constexpr int kNodePayloadSecond = 10980202;// EXTRA_ACTION 后手侧

int storage_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second);
}

// 本回合 owner 所选技能是否为万皇宗魄决（MOVE_RIGHT 时 roundChoice 已写入）。
bool selected_is_30381(BattleContext* ctx, int owner) {
    constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL
    if (!ctx || owner < 0 || owner > 1 || ctx->roundChoice[owner][0] != kActionSelectSkill) {
        return false;
    }
    const int slot = ctx->roundChoice[owner][1];
    const auto& skills = ctx->getPet(owner).skills;
    if (slot < 0 || slot >= static_cast<int>(skills.size())) {
        return false;
    }
    return skills[static_cast<std::size_t>(slot)].id == kSkillId;
}

// ── MOVE_RIGHT 常驻节点：暴击梯度武装（只在本回合选 30381 时写，不波及其它技能）──
EffectResult effect_1098_crit_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || !selected_is_30381(ctx, owner)) {
        return EffectResult::kOk;
    }
    // 本回合的这次使用是第 uses+1 次（次数在 SKILL_EFFECT 才 +1，晚于掷点）。
    const int uses = storage_int(ctx, kUsesKeyBase + owner);
    ctx->ws.crit_rate_add[owner] =
        static_cast<float>(std::min((uses + 1) * kCritStep, kCritCap));
    return EffectResult::kOk;
}

// ── MOVE_RIGHT 常驻节点：星皇之怒载荷消费（下2次攻击技先制+2）──
EffectResult effect_1098_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kChargesKeyBase + owner;
    const int charges = storage_int(ctx, key);
    if (charges <= 0 || !selected_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;   // 属性技不耗层不加先制
    }
    ctx->ws.preemptive_level[owner] += kPriorityBonus;
    ctx->plugin_storage[key] = charges - 1;
    return EffectResult::kOk;
}

// ── AFTER_ACTION_END（rounds=1）：技能结束 → 对手存活 → 50% 声明额外行动 ──
EffectResult effect_1098_trigger(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || !used_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(1 - owner).hp <= 0) {
        return EffectResult::kOk;   // "对手体力值高于0"
    }
    if (!roll_percent(ctx->rng(), kTriggerPct)) {
        return EffectResult::kOk;
    }
    ctx->declare_extra_action(owner);
    return EffectResult::kOk;
}

// ── EXTRA_ACTION（rounds=1）：额外行动里触发星皇之怒 → 载荷 +2 层 ──
EffectResult effect_1098_wrath_payload(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kChargesKeyBase + owner;
    ctx->plugin_storage[key] = storage_int(ctx, key) + kWrathCharges;
    return EffectResult::kOk;
}

// ── SKILL_EFFECT 本体：记账 + 节点武装 ──
EffectResult effect_1098_wanhuang_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // ① 梯度记账（本次掷点已过——首用偏差见文件头）。
    ctx->plugin_storage[kUsesKeyBase + owner] = storage_int(ctx, kUsesKeyBase + owner) + 1;
    // ② 常驻节点一次性注册（999 回合窗口≈战斗长；重复用技不再叠注册）。
    if (storage_int(ctx, kArmedKeyBase + owner) == 0) {
        register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, kNodeCritArm,
                              kBattleLongRounds, {owner}, &effect_1098_crit_arm);
        register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, kNodePriority,
                              kBattleLongRounds, {owner}, &effect_1098_priority_tick);
        ctx->plugin_storage[kArmedKeyBase + owner] = 1;
    }
    // ③ 本回合的触发/载荷节点：rounds=1 只活在用技当回合（InRounds、时长<2 不做后手顺延
    //    折算 → 先后手两侧的本回合 AFTER_ACTION_END/EXTRA_ACTION 都盖得住），每用重挂
    //    （同键替换=刷新，不叠加）。FIRST/SECOND 双侧都挂：桶按 (时点, 行动方) 执行，
    //    非行动方一侧自然空跑。
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION_END, owner, kNodeTriggerFirst,
                          1, {owner}, &effect_1098_trigger);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION_END, owner, kNodeTriggerSecond,
                          1, {owner}, &effect_1098_trigger);
    register_round_effect(ctx, State::BATTLE_FIRST_EXTRA_ACTION, owner, kNodePayloadFirst,
                          1, {owner}, &effect_1098_wrath_payload);
    register_round_effect(ctx, State::BATTLE_SECOND_EXTRA_ACTION, owner, kNodePayloadSecond,
                          1, {owner}, &effect_1098_wrath_payload);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1098, effect_1098_wanhuang_arm);
