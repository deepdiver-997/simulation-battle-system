// K265 随机属性+1族 —— effect 382（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 382，2026-10-21）：
//   382 冥翼天魂 2801 args[5,1]（kind=[11]）：「每次使用攻击技能都能使自身随机属性+1」
//     → **args 布局不消费（记档）**：本行**无兄弟行可锚**（载体 2801 仅此一行），
//       且同族"随机 N 项能力值+1"各行的 **N 位置互不相同**——954 db[2]=3、
//       1281 db[2]=3、1056 db[1]=2、1601 db[0]=2、370 db[1]=1、682 db[0]=2、
//       622 db[0]=1（K4 头注："k_allowed 布局互不通用"）。本行 db[0]=5 / db[1]=1
//       无法自洽解码（db[1]=1 可能是"项数 1"也可能是"等级 +1"；db[0]=5 语义不明）
//       → 按 K2 纪律**解析不了宁可不用、不猜**：数值取 tips 字面（**随机 1 项**属性、
//       **+1** 等级），db 原样记档备查。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   触发源 = **EVENT_SKILL_USED**（技能**使用**即发，miss/被封也算——E13 锚点
//     语义）——官方"每次使用**攻击技能**"锚在使用时点。
//     门 = ev.actor == 印主（自己使用）且 ev.used_attribute == false（**攻击技**门，
//     K2 358 / K164 109/116/151 同款；E13 载荷 used_attribute = 声明类型是属性技）。
//   效果 = 随机槽 stat_change(印主, sbs_rand()%6, +1)（自身增益通道，**不查免弱**——
//     stat_drop 才查免弱；K196 390 / K255 320 / K 板 622 同款随机槽写法：
//     `g_core->stat_change(ctx, owner, sbs_rand() % 6, 1)`）。
//     ⚠️ 等级上限 ±6 由原语内钳。
//   ⚠️ **actor 门（ev.actor != owner）未被场景覆盖**：事件 watcher 按监听方 owner
//     注册，EVENT_SKILL_USED 的投递目标使对手的技能事件不投给印主 → 印主的
//     watcher 对对手的用技**零调用**（2026-10-21 实测：破坏 actor 门不改变
//     T4212 结果）→ 该守卫保留但记档为未覆盖。
//   ⚠️ watcher 登记在 arm 节点（early 幂等旗，K164 / K178 351 同款）——
//     **跨回合 watcher 必须 WatcherScope::TEAM**（ON_STAGE 的 valid_id 活不过
//     回合交界，常识速查 §五）。
//   ⚠️ 随机槽口径：sbs_rand()%6 覆盖 0攻/1特攻/2防/3特防/4速/5命中 六槽（K 板 622
//     同款），**可重复选中同一槽**（官方"随机属性"未说去重；k893 随机降属用去重
//     循环是另一族口径——记档）。
//
// ⚠️ 三查记录（author a03-kprod 2026-10-21）：①内核 grep 382 无魂印行为硬编码；
//    ②effect_icon kind=[11]、tips 为权威文（dump 原文已核，effect_info 无 382
//    技能模板）；③registry soul 集合无 382、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k_round_stat 622（随机槽 stat_change +1 同款）、k_attack_prob 109/116/151
//   （EVENT_SKILL_USED 攻击技窗）、k_anomaly_chance 358（arm 幂等旗 + TEAM watcher）。
// 场景回归：test/scenario/scenario_4210_random_stat_382.cpp（号段 4210-4219）。

#include "common.h"

#include <cstdlib>

namespace {

constexpr int kMarkId = 382;
constexpr int kWiredKeyBase = 990265000;   // +owner：watcher 已挂旗标

// arm：挂 EVENT_SKILL_USED watcher（自己用攻击技 → 随机属性 +1）。
EffectResult k265_382_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int wired_key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(wired_key)) {
        return EffectResult::kOk;   // early 双触发防重
    }
    ctx->plugin_storage.emplace(wired_key, 1);

    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->stat_change) {
                return;
            }
            if (ev.actor != owner) {
                return;   // 只认自己使用
            }
            if (ev.used_attribute) {
                return;   // 攻击技门（属性技不触发）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const int slot = c->rng().next() % 6;   // 0攻 1特攻 2防 3特防 4速 5命中
            (void)g_core->stat_change(c, owner, slot, /*delta=*/1);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（382 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k265_382_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
