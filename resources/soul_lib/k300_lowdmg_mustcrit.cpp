// K300 伤害阈值必暴族 —— effect 1223（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 1223，2026-10-21）：
//   1223 篝火大使 3641 args[300,2]（kind=[1]）：「自身攻击技能造成的伤害低于300则
//     下2回合必定打出致命一击（boss有效）」
//     → **db[0]=300 = 伤害阈值**（tips 字面"伤害低于300"自证）；
//       **db[1]=2 = 窗口回合数**（tips 字面"下2回合"自证）。
//     ⚠️ 参数自证（同族行交叉锚定）：
//       ①1301 args[300,1,300,1]「自身攻击造成的伤害低于300则下回合攻击必定打出
//         致命一击…」——db[0]=300 阈值同位同义；其"下回合"= 1 回合 → 本行 db[1]=2
//         对应"下2回合"；
//       ②1341 args[300,100,300,1,100,1]「…伤害低于300则下回合…攻击100%打出致命
//         一击」——db[0]=300 阈值同位；
//       ③1164 args[2,1,280,3,100,3,150,1]「…若本次攻击造成的伤害低于280则…
//         令自身下回合攻击技能必定打出致命一击」——db[2]=280 阈值同位。
//       → 三个同族行的阈值位一致 + 回合数位自洽 → 本行双参均自证。
//     ⚠️ **（boss有效）**尾注 → 节点 boss_invalid=**false**（boss 挑战对局也执行）。
//       与"（boss无效）"措辞相反——"boss有效"明确 = 对 boss 也生效。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ⚠️ **必须走 db_at**（读宿主 pet.soulMark.args.owned_int_args 原序），不能走
//      a.row（类型化 SoulNodeArgs）——后者在**真实对局里是空的**（bind_soulmark_args
//      丢弃 owned_int_args 前两位，而真实对局的 owned_int_args 就是 DB args 原样，
//      通常 2~8 位）→ a.row = owned[2:] 常常为空。存量 17 个用 a.row_or 的文件全靠
//      **fallback 硬编码**兜底，测试能过是因为场景按 {0,0,...行参} 构造（多带两位
//      占位）。本单实现与场景**两边都对**（K257 起的夜跑口径）。
//
//   ① **伤害判定**（EVENT_TAKE_DAMAGE watcher，伤害**落地后**）：
//      官方"自身攻击技能造成的伤害" = **终伤红伤实际落地值**（encarlos_3577 1164
//      同款口径，用户 2026-09-24 拍板：被护盾挡住的部分不算）。本单用事件面的
//      `ev.amount` = **实际扣血量**（battle_primitives.cpp:988「受到伤害事件…
//      amount = 实际扣血」）——直接就是官方口径，无需 hp 快照差值。
//      门 = ①ev.actor == owner（**自己打的**）②ev.target == 对手 ③ev.amount > 0
//      ④本次用的是**攻击技**（tips 明写"自身攻击技能"）⑤**实际扣血 < db[0]**
//      （严格小于，"低于300"不含等于；与 1301/1164 同款边界口径）。
//      ⚠️ 攻击技门用 **ev.state** 判（事件带 emit 时的 FSM 时点，watcher 在 drain
//      时跑、currentState 已推进，靠它反推是攻击伤害）——battle_primitives.cpp:986。
//         攻击伤害的 state ∈ {BATTLE_FIRST/SECOND_ATTACK_DAMAGE} 及其邻域；用
//         "本次所选技是攻击技"判更稳（roundChoice 读法）。
//   ② **窗口**：命中后记"必暴窗口" = 从**下一回合**起 db[1] 个回合
//      （起始 = ctx->roundCount + 1，长度 = db[1]）。
//   ③ **消费**（BATTLE_FIRST/SECOND_BEFORE_SKILL_HIT 时点）：若当前回合落在
//      窗口内 → ws.must_crit[owner] = true。
//      ⚠️ ws.must_crit **每回合 reset**、在**命中时点消费**（skills.cpp:948：
//      `crit_happened = must_crit || …`）——故必须**每回合重写**（窗口内每回合
//      的命中前桶都写一次），不能只写一次。encarlos_3577「下回合必暴」同款。
//      ⚠️ 窗口按回合数计，印主不出手则自然过期。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 1223 无魂印行为硬编码；
//    ②effect_icon kind=[1]、tips 为权威文（dump 原文已核；effect_info 无 1223
//    技能模板）；③registry soul 集合无 1223、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：encarlos_light_3577 1164（**终伤判定 + "下回合必暴"标记 + 命中前桶消费
//   ws.must_crit，直系同款**——本单是它的多回合版）、k199_hit_clear_boost
//   （EVENT_TAKE_DAMAGE watcher 判据：ev.actor/ev.target/ev.amount 读法）、
//   k_round_recover_family 227（ws.must_crit 旗 + 每回合 reset 套路）。
// 场景回归：test/scenario/scenario_4560_lowdmg_mustcrit_1223.cpp（号段 4560-4569）。

#include "common.h"

#include <entities/skills.h>

namespace {

constexpr int kMarkId = 1223;
constexpr int kThresholdFallback = 300;   // tips 字面"伤害低于300"（db[0]）
constexpr int kRoundsFallback = 2;        // tips 字面"下2回合"（db[1]）

// plugin_storage 段 1223xxxx（+owner 区分双方）。
constexpr int kWindowStartKey = 1223001;   // +owner：必暴窗口起始回合（= 命中回合 + 1）
constexpr int kWindowRoundsKey = 1223002;  // +owner：窗口长度（db[1]）

// ── 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致）────────
struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, int owner, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = ctx->find_pet_with_soulmark(owner, mark_id);
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

// 所选技是不是攻击技（tips 明写"自身攻击技能"）——roundChoice 读法。
bool used_attack_skill(BattleContext* ctx, int owner) {
    if (!ctx || ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return false;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5) {
        return false;
    }
    return ctx->getPet(owner).skills[idx].type != SkillType::Attribute;
}

// ① arm：挂 EVENT_TAKE_DAMAGE watcher（自己打出的低伤 → 开必暴窗口）。
EffectResult k300_1223_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int wired_key = kWindowRoundsKey + 10000 + owner;   // 独立的"已挂线"旗位
    if (ctx->plugin_storage.count(wired_key)) {
        return EffectResult::kOk;   // early 双触发防重
    }
    ctx->plugin_storage.emplace(wired_key, 1);

    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner || ev.amount <= 0) {
                return;   // 只认**自己打出**的伤害（ev.target = 被打方）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            // ⚠️ 攻击技门（tips 明写"自身攻击技能"）在此**冗余但无害**：
            //   本 watcher 挂 EVENT_TAKE_DAMAGE——该事件**只对红伤/真伤发**
            //   （属性直伤走 deal_attribute_damage、不发本事件；粉伤走自己的
            //   EVENT_TAKE_PINK_DAMAGE）→ 属性技根本走不到这个门。
            //   保留该门是为了"万一将来红伤通道扩展到属性技"时仍符合字面，
            //   但**它在本单场景里不可观测**（不是覆盖缺口，是通道事实）。
            if (!used_attack_skill(c, owner)) {
                return;   // 攻击技门（冗余，见上注）
            }
            const HostRow row = host_row(c, owner, kMarkId);
            const int threshold = row.valid ? db_at(row, 0, kThresholdFallback)
                                           : kThresholdFallback;
            const int rounds = row.valid ? db_at(row, 1, kRoundsFallback)
                                        : kRoundsFallback;
            if (threshold <= 0 || rounds <= 0) {
                return;
            }
            if (ev.amount >= threshold) {
                return;   // 门闭：实际扣血不低于阈值（"低于"是严格小于）
            }
            // 命中低伤 → 开窗口：起始 = **下一回合**，长度 = db[1]。
            // ⚠️ roundCount 基准：advanceRound 在 BATTLE_ROUND_COMPLETION 时调用
            //   （battleFsm.cpp:2323）→ "第 N 回合的 ROUND_START 时 roundCount == N-1"。
            //   本 watcher 跑在 damage 事件的 drain 时点——**该回合已结束、roundCount
            //   已被 +1**（第 N 回合伤害落地时读到的是 N）。故"下一回合"的 ROUND_START
            //   值恰为**当前 roundCount**（不是 +1）。
            c->plugin_storage[kWindowStartKey + owner] = c->roundCount + 1;
            c->plugin_storage[kWindowRoundsKey + owner] = rounds;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ③ BEFORE_SKILL_HIT：窗口内 → ws.must_crit = true（命中时点消费，每回合重写）。
EffectResult k300_1223_before_skill_hit(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const auto start_it = ctx->plugin_storage.find(kWindowStartKey + owner);
    const auto len_it = ctx->plugin_storage.find(kWindowRoundsKey + owner);
    if (start_it == ctx->plugin_storage.end() || len_it == ctx->plugin_storage.end()) {
        return EffectResult::kOk;   // 无窗口
    }
    const int start = any_int_or(start_it->second, -1);
    const int len = any_int_or(len_it->second, 0);
    if (len <= 0 || ctx->roundCount < start || ctx->roundCount >= start + len) {
        return EffectResult::kOk;   // 不在窗口内（窗口 = [start, start+len)）
    }
    ctx->ws.must_crit[owner] = true;   // 命中时点消费（skills.cpp:948）
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1223 入账，只增不改）──────
// ⚠️ boss_invalid=**false**（tips 明写"（boss有效）"）。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k300_1223_arm,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_FIRST_BEFORE_SKILL_HIT, &k300_1223_before_skill_hit,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_SECOND_BEFORE_SKILL_HIT, &k300_1223_before_skill_hit,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
