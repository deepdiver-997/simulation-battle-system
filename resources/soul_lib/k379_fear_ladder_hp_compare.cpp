// K379 概率爬坡害怕+体力对照增伤/回血族 —— effect 1381（1 宏 / 2 行）
//（天石灵皇·阿洛比斯 3803 row1078 本体 + row1648 伙伴行；号段 7260-7269，
//  worktree ../agents/k379，2026-10-24）
//
// 行清单（tools/soul_effects_dump.py 1381，2026-10-24）：
//   row1078 args[40,100,3,1] kind[1,5,10]：
//     「自身使用攻击技能40%令对手害怕，未触发则下次触发的概率翻倍；
//       回合开始时，若自身体力高于对手则当回合造成的攻击伤害提升100%，
//       若自身体力低于对手则战斗阶段结束时恢复自身最大体力的1/3（boss无效）」
//   row1648 args[50,120,2,1]（四灵归位伙伴效果）：同文 40→50、100→120、3→2。
//
// ⚠️ 三查记录（author 立单 2026-10-24 / a76-jprod 动工同日复核）：
//   ①内核 grep \b1381\b 无魂印行为硬编码；②effect_info 1381 技能空间模板异文
//   （tips 权威）；③registry soul 无 1381、soul_lib 全库 grep 无同 id 注册——真缺口。
//
// 行参解码（两行同构、伙伴行随行变强锚自证）：
//   db[0]=害怕基础率%（40/50）、db[1]=增伤%（100/120）、db[2]=回血分母（3/2）、
//   db[3]=1 旗（不消费——K1 纪律）。
//
// 实现（板行处方）：
//   ① 概率爬坡害怕：EVENT_SKILL_USED watcher（own + 攻击技门，miss 也算——E13 锚，
//     k265/k893 同门；TEAM 常驻，early arm 幂等防重）→ 申报 pct = min(base×2^连败, 100)
//     → apply_anomaly_checked(对手, 害怕6, random_anomaly_duration(), owner, pct,
//     SoulMark)（概率申报内核单掷，禁本地双掷——k_attack_prob 口径）。
//     "未触发" = ApplyAnomalyResult 非 SUCCESS/DURATION_EXTENDED/REPLACED_EXISTING
//     档（ROLL_FAILED 即官方"未触发补偿"认档——battle_primitives.h 枚举注记；
//     免疫/被挡也按未触发计，记档）→ 连败++；触发档 → 连败清零。
//     F9 审计槽（990379000+owner 记本次申报 pct，K2 662 同款——场景无随机梯度断言）。
//   ② 回合开始体力对照：ROUND_START 严比较（高于→ws.damage_add_pct[owner]+=db[1]
//     当回合增伤（ws 每回合 reset→单回合窗，d5_D4/K363 重写套路）；低于→记待回血
//     回合戳；相等→两者皆无——tips 严格"高于/低于"）→ ROUND_END 读戳 heal(max/db[2])
//     （K338/chaos_sorensen heal_amount 原语，内部钳满）。
// 注册：SOUL_PROGRAM(1381) 三节点（arm + ROUND_START + ROUND_END），
// SOUL_PROGRAM 总数 +1（只增不改）；tips 尾注（boss无效）→ 全节点 boss_invalid=true。
// 场景回归：test/scenario/scenario_7260_fear_ladder_hp_compare.cpp（7260-7269）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1381;
constexpr int kFearId = static_cast<int>(AbnormalStatusId::Fear);
constexpr int kAuditChanceKeyBase = 990379000;   // +owner：本次申报 pct（F9 审计槽，只读）
constexpr int kWiredKeyBase = 990379100;         // +owner：watcher 已挂旗标

// ── 跨回合账本（soulmark_storage 容器，K218/K363 同款）─────────────────────────
struct K379Runtime {
    int fear_streak = 0;    // 害怕连败数（申报概率 = base×2^连败，触发清零）
    int heal_round = -1;    // 待回血回合戳（ROUND_START 低于门置位，ROUND_END 消费）
};

K379Runtime& k379_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K379Runtime{};
    }
    return std::any_cast<K379Runtime&>(storage[kMarkId]);
}

// ── 宿主行参数（db_at 原序读 owned_int_args——K279/K363 同款）──────────────────
struct HostRow {
    const std::vector<int>* db = nullptr;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const SoulNodeArgs& a, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// ── arm：挂 EVENT_SKILL_USED watcher（概率爬坡害怕）───────────────────────────
EffectResult k379_1381_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->plugin_storage.count(kWiredKeyBase + owner)) {
        return EffectResult::kOk;   // early 双触发防重（K164/K178 同款）
    }
    ctx->plugin_storage.emplace(kWiredKeyBase + owner, 1);

    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner || ev.used_attribute) {
                return;   // own + 攻击技门（属性技不申报——E13 载荷声明类型）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const int opp = 1 - owner;
            if (c->getPet(opp).hp <= 0) {
                return;
            }
            int base_pct = 40;
            {
                const int slot = c->find_pet_with_soulmark(owner, kMarkId);
                if (slot >= 0 && slot <= 5) {
                    const auto& db =
                        c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
                    if (!db.empty() && db[0] > 0) {
                        base_pct = db[0];
                    }
                }
            }
            K379Runtime& rt = k379_runtime(c, owner);
            // 申报 pct = min(base×2^连败, 100)（逐次倍增、封顶即止——无溢出）。
            int pct = base_pct;
            for (int i = 0; i < rt.fear_streak && pct < 100; ++i) {
                pct *= 2;
            }
            if (pct > 100) {
                pct = 100;
            }
            c->plugin_storage[kAuditChanceKeyBase + owner] = pct;   // F9 审计槽
            const ApplyAnomalyResult res = g_core->apply_anomaly_checked(
                c, opp, kFearId, random_anomaly_duration(), owner, pct,
                ChanceSource::SoulMark);
            if (res == ApplyAnomalyResult::SUCCESS ||
                res == ApplyAnomalyResult::DURATION_EXTENDED ||
                res == ApplyAnomalyResult::REPLACED_EXISTING) {
                rt.fear_streak = 0;   // 触发 → 爬坡归零
            } else {
                ++rt.fear_streak;     // 未触发（含 ROLL_FAILED/免疫/被挡，记档）
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── ② ROUND_START：体力对照门 ────────────────────────────────────────────────
EffectResult k379_1381_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    K379Runtime& rt = k379_runtime(ctx, owner);
    const int my_hp = ctx->getPet(owner).hp;
    const int foe_hp = ctx->getPet(1 - owner).hp;
    if (my_hp > foe_hp) {
        const int amp = db_at(row, 1, 0);
        if (amp > 0) {
            ctx->ws.damage_add_pct[owner] += static_cast<float>(amp);
        }
        rt.heal_round = -1;
    } else if (my_hp < foe_hp) {
        rt.heal_round = ctx->roundCount;   // 待回血戳（本轮 ROUND_END 消费）
    } else {
        rt.heal_round = -1;   // 相等：两支皆无（tips 严格高于/低于）
    }
    return EffectResult::kOk;
}

// ── ③ ROUND_END：低体回血（max/db[2]）────────────────────────────────────────
EffectResult k379_1381_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    K379Runtime& rt = k379_runtime(ctx, owner);
    if (rt.heal_round < 0 || rt.heal_round != ctx->roundCount) {
        return EffectResult::kOk;
    }
    rt.heal_round = -1;
    const HostRow row = host_row(ctx, a, kMarkId);
    const int heal_div = db_at(row, 2, 0);
    if (heal_div <= 0) {
        return EffectResult::kOk;
    }
    const int max_hp = resolve_pet_max_hp(ctx->getPet(owner));
    const int heal = max_hp / heal_div;
    if (heal > 0) {
        (void)g_core->heal_amount(ctx, owner, heal);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1381 入账，只增不改）──────
// tips 尾注（boss无效）→ 全节点 boss_invalid=true。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k379_1381_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &k379_1381_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k379_1381_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
