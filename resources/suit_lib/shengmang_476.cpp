// 圣芒佑界套装（套装 id 476，部件 1301154-1301157 圣芒佑界战盔/臂铠/腰带/战靴）
//
// 官方全文（equip.bytes desc；suit.bytes id=476）：
//   己方精灵体力+100点，其他能力+80点；己方在场精灵抗性中的唯一最高项触发概率
//   提升至100%，其余项概率减少至0%；自身进入的异常状态最高为2回合；自身异常抗性
//   触发时，恢复自身最大体力的30%，每生效1次比例减少5%，下场时重置
//   （PVE无效，圣芒佑界战盔额外效果：己方精灵命中率提升15%）
//
// 拆解（数值部分走 custom_equip_stats 数据层，见 suit.cpp resolve_equipment）：
//   A 体力+100、其他+80        → 数据层 per_suit 行（不动效果）
//   B 抗性唯一最高项→100%、其余→0%（己方在场精灵）
//       → ROUND_START 重断言 + EVENT_ENTER_STAGE 监听（换宠跟随）。
//         变换幂等（100/0 重算结果不变），离场不还原——抗性只被施加判定消费，
//         场下精灵无消费点，重登再断言，可观测行为等价。
//         并列最高（无"唯一"项）→ 条件不成立，不动抗性。
//   C 异常持续上限 2 回合       → ctx->anomaly_duration_cap[side]（激活时装一次；
//         apply_anomaly_impl 统一钳制，clearAllEffects 随对局复位）
//   D 抗性触发回血 30%，每次 -5pp，下场重置
//       → EVENT_ANOMALY_RESISTED TEAM 监听（apply_anomaly 抗性分支直发，语义唯一）；
//         次数存 pet.on_stage_storage（引擎在换宠时清空 = 官方"下场时重置"）。
//   E 命中率提升15%（战盔额外）→ ws.hit_rate_mod[side] = 1.15（ROUND_START 每回合写；
//         该字段此前无消费点，引擎已在 query_usage 精度乘算链接入）
//
// PVE 无效条款不建模（本引擎只模拟赛尔间对战）。
// 生命周期：全部 TEAM 常驻（套装纪律，见 suit_reg.h）。

#include "common.h"

#include <abnormal-system/resistance-system.h>

namespace {

constexpr int kSuitShengmang = 476;
constexpr int kNodeInstall = 0;     // early：C 上限 + B 换宠跟随 + D 监听（幂等）
constexpr int kNodeRoundStart = 1;  // E 命中率 + B 重断言
// on_stage_storage 键：D 的"抗性触发次数"（下场由引擎清空 = 重置）。
constexpr int kResistCountKey = 476;

// ── B：抗性唯一最高项 → 100%，其余非零项 → 0%（无唯一最高则不动）────────
// 抗性槽 = control[3] + uncontrol[3]（{anomaly_id, percent}）；complete_immunity
// 是独立维度不参与"项"。变换幂等：已变换后最高项仍是同一 id（100），重算不变。
void transform_on_stage_resistance(BattleContext* ctx, int owner) {
    ElfPet& pet = on_stage_pet(ctx, owner);
    ResistanceSystem& rs = pet.resistance;

    // 收集非零项（槽位类别 + anomaly_id + 当前百分比）
    struct Item { bool control; int slot; int id; int pct; };
    Item items[kResistanceSlotCount * 2];
    int n = 0;
    for (int i = 0; i < kResistanceSlotCount; ++i) {
        const auto c = rs.getControlSlot(i);
        if (c.second > 0) {
            items[n++] = Item{true, i, c.first, c.second};
        }
        const auto u = rs.getUncontrolSlot(i);
        if (u.second > 0) {
            items[n++] = Item{false, i, u.first, u.second};
        }
    }
    if (n == 0) {
        return;   // 无抗性项 → 无"最高项"
    }
    int max_pct = -1;
    int max_count = 0;
    int max_idx = -1;
    for (int i = 0; i < n; ++i) {
        if (items[i].pct > max_pct) {
            max_pct = items[i].pct;
            max_count = 1;
            max_idx = i;
        } else if (items[i].pct == max_pct) {
            ++max_count;   // 已变换后的 0 项不进 items，并列只可能来自原生并列
        }
    }
    if (max_count != 1) {
        return;   // 无"唯一"最高项 → 条件不成立
    }
    for (int i = 0; i < n; ++i) {
        const int target_pct = (i == max_idx) ? 100 : 0;
        if (items[i].pct == target_pct) {
            continue;   // 幂等：已是目标值不重写（也避免 setControlSlot 校验噪声）
        }
        if (items[i].control) {
            rs.setControlSlot(items[i].slot, items[i].id, target_pct);
        } else {
            rs.setUncontrolSlot(items[i].slot, items[i].id, target_pct);
        }
    }
}

// ── E：命中率提升 15% + B 重断言（每回合）───────────────────────────────
EffectResult shengmang_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.hit_rate_mod[owner] = 1.15f;
    transform_on_stage_resistance(ctx, owner);
    return EffectResult::kOk;
}

// ── C 上限 + B 换宠跟随 + D 监听的安装节点（early，幂等）─────────────────
EffectResult shengmang_install(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kSuitShengmang * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[key] = true;

    // C：己方进入的异常持续 ≤ 2 回合（battle 级，clearAllEffects 复位）。
    ctx->anomaly_duration_cap[owner] = 2;

    // B 换宠跟随：EVENT_ENTER_STAGE 由 perform_switch 统一 emit（主动中切 + 死亡换宠
    // 两条路径，actor=登场方）→ 新登场精灵立刻断言抗性变换。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE,
        owner,
        ctx->roundCount,
        /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner) {
                return;   // 只认己方登场
            }
            transform_on_stage_resistance(wctx, owner);
        },
        WatcherScope::TEAM,
        /*valid_id=*/0
    );

    // D：抗性触发 → 回血 30%（每次生效 -5pp，最低 0），计数挂在
    // on_stage_storage——引擎在换宠时清空该槽 = 官方"下场时重置"。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ANOMALY_RESISTED,
        owner,
        ctx->roundCount,
        /*duration_rounds=*/0,   // 永久
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core) {
                return;
            }
            if (ev.target != owner) {
                return;   // 只认己方精灵的抗性触发
            }
            ElfPet& resister = on_stage_pet(wctx, owner);
            const int count = [&] {
                auto it = resister.on_stage_storage.find(kResistCountKey);
                return it == resister.on_stage_storage.end()
                           ? 0 : any_int_or(it->second);
            }();
            int pct = 30 - 5 * count;
            if (pct <= 0) {
                // 次数用尽不再回血；计数仍递增（口径：多次触发比例递减到 0 为止）
                resister.on_stage_storage[kResistCountKey] = count + 1;
                return;
            }
            resister.on_stage_storage[kResistCountKey] = count + 1;
            const int max_hp = resolve_pet_max_hp(resister);
            if (max_hp <= 0) {
                return;
            }
            g_core->heal_amount(wctx, owner, max_hp * pct / 100);
        },
        WatcherScope::TEAM,
        /*valid_id=*/0
    );
    return EffectResult::kOk;
}

}  // namespace

SUIT_PROGRAM(kSuitShengmang,
             SoulMarkNodeRef(State::BATTLE_ROUND_START, &shengmang_install,
                             /*once=*/false, /*early=*/true),
             SoulMarkNodeRef(State::BATTLE_ROUND_START, &shengmang_round_start))
