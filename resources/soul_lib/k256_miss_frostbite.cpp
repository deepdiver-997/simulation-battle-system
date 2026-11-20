// K256 对手攻击miss→冻伤族 —— effect 346（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 346，2026-10-20）：
//   346 雪色麒麟 2707 args[1,5]（kind=[]）：「若对手攻击miss则对手冻伤」
//     → **args 布局不消费（记档）**：K4 头注定版「k_allowed 布局互不通用」
//       （636/641/642 三种布局并存）；本行 2 位试解皆不自洽——按 641 布局
//       (delta,slot) 解作 (1,5)=攻击+1/命中槽+1，与文本完全对不上；按 346 的
//       文本只有"冻伤"一个动作、无等级数值。**db[1]=5 恰是官方异常码
//       Frostbite=5**（常识速查 §三 异常码表），构成弱印证——但按 K2 纪律
//       （解析不了宁可不用、不猜）仍以 tips 字面落地，参数记档备查。
//
// ⚠️ 实现形态（T4 类型化行参）：
//   触发源 = **EVENT_SKILL_INVALID**（"技能无效/未命中"，Skills::execute 中 emit，
//     target = 对方）——这是引擎侧"miss"的唯一事件面。
//   门 = ev.actor == 对手（1-owner）且对手本回合所选技是**攻击技**
//     （skills[idx].type != SkillType::Attribute）——官方文本是"对手**攻击**miss"，
//     属性技 miss 不算（k_spacetime_leap 1320 攻击技过滤同款）。
//     所选技下标读 `ctx->roundChoice[opp][1]`（roundChoice 选择位口径，常识速查 §四）。
//   效果 = apply_anomaly(冻伤 Frostbite=5)，时长 random_anomaly_duration()（= 2 回合，
//     inline 恒定值——确定性可断言）。
//   ⚠️ watcher 登记在 arm 节点（early 幂等旗，k_spacetime_leap 1673 / k_anomaly_chance
//     262 同款）——**跨回合 watcher 必须 WatcherScope::TEAM**（ON_STAGE 的 valid_id
//     活不过回合交界，常识速查 §五）。
//   ⚠️ EVENT_SKILL_INVALID 也覆盖"技能无效"（被封/被盔），不只 miss——官方文本只说
//     miss，但引擎该事件不区分二者（**口径记档**：本实现按"对手攻击技未命中"处理，
//     技能无效同事件同处理；若后续实盘推翻，改此处一处即可）。
//
// ⚠️ 三查记录（author a01-kprod 2026-10-21）：①内核 grep 346 无魂印行为硬编码；
//    ②effect_icon kind=[]、tips 为权威文（dump 原文已核，effect_info 无 346
//    技能模板）；③registry soul 集合无 346、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k_spacetime_leap 1673（EVENT_SKILL_INVALID 攻击技过滤 watcher 旗舰）、
//   k193_cond_freeze（冻伤施加通道）、k_anomaly_chance 262（arm 幂等旗 + TEAM watcher）。
// 场景回归：test/scenario/scenario_4120_miss_frostbite_346.cpp（号段 4120-4129）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kMarkId = 346;
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);  // 5

// arm：挂 EVENT_SKILL_INVALID watcher（对手攻击技未命中 → 对手冻伤）。
EffectResult k256_346_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // arm 幂等（early 节点开局双触发防重）：用 plugin_storage 旗。
    const int wired_key = 990256000 + owner;
    if (ctx->plugin_storage.count(wired_key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(wired_key, 1);

    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly) {
                return;
            }
            const int opp = 1 - owner;
            if (ev.actor != opp) {
                return;   // 只认对手的未命中
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            // 门：对手本回合所选技是攻击技（"对手**攻击**miss"）。
            const int idx = c->roundChoice[opp][1];
            const auto& skills = c->getPet(opp).skills;
            if (idx < 0 || idx >= static_cast<int>(skills.size())
                || skills[static_cast<std::size_t>(idx)].type == SkillType::Attribute) {
                return;   // 属性技 miss 不算
            }
            (void)g_core->apply_anomaly(c, opp, kFrostbite, random_anomaly_duration(), owner);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（346 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k256_346_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
